#include <algorithm>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <driver/i2s.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "server.h"
#include "mic.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MIC", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

// Apenas 1 por eTomada
static Microfone mic = {.ligado = false};

#define SAMPLE_RATE 16000
#define DURACAO_SEG 3
#define I2S_PORT I2S_NUM_0

static constexpr size_t MIC_TAMANHO_WAV =
    44 + SAMPLE_RATE * DURACAO_SEG * sizeof(int16_t);

static uint8_t *micWav = nullptr;
static std::atomic_flag micLiberando = ATOMIC_FLAG_INIT;
static std::atomic<bool> micOcupado{false};
static std::atomic<bool> micTaskAtiva{false};
static std::atomic<bool> micHTTPAtivo{false};
static std::atomic<bool> micCancelado{false};
static std::atomic<bool> micFinalizado{false};

Microfone *micGet()
{
  return &mic;
}

bool micAtivo()
{
  return hardwareProfile.mic.sckPin != 255;
}

void micInit()
{
  if (!micAtivo())
    return;

  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = hardwareProfile.mic.sckPin;
  pins.ws_io_num = hardwareProfile.mic.wsPin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = hardwareProfile.mic.sdPin;

  esp_err_t err = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
  if (err != ESP_OK)
  {
    logaM(LOG_CRITICO, "Erro i2s_driver_install: %d", err);
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK)
  {
    logaM(LOG_CRITICO, "Erro i2s_set_pin: %d", err);
    i2s_driver_uninstall(I2S_PORT);
    return;
  }

  mic.ligado = true;

  logaM(LOG_NORMAL, "Microfone Encontrado!");
}

JsonDocument micGetJSONDoc(Recurso *r, bool full)
{
  JsonDocument doc;

  Microfone *m = r->mic;
  if (!m)
    return doc;

  doc["ligado"] = m->ligado;

  return doc;
}

static int32_t amostras32[256];
static int16_t amostras16[256];

// Converte INMP441 24-bit (slot 32-bit) para PCM16.
// Shift 14 aplica ganho digital de aproximadamente 12 dB.
static int16_t converteAmostra(int32_t valor)
{
  int32_t pcm = valor >> 14;

  if (pcm > 32767)
    pcm = 32767;
  if (pcm < -32768)
    pcm = -32768;

  return (int16_t)pcm;
}

static void le16(uint8_t *p, uint16_t v)
{
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
}

static void le32(uint8_t *p, uint32_t v)
{
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

static void wavHeader(uint8_t *h, uint32_t bytesAudio)
{
  memset(h, 0, 44);

  memcpy(h, "RIFF", 4);
  le32(h + 4, bytesAudio + 36);

  memcpy(h + 8, "WAVEfmt ", 8);
  le32(h + 16, 16);
  le16(h + 20, 1); // PCM
  le16(h + 22, 1); // Mono

  le32(h + 24, SAMPLE_RATE);
  le32(h + 28, SAMPLE_RATE * 2);

  le16(h + 32, 2);
  le16(h + 34, 16);

  memcpy(h + 36, "data", 4);
  le32(h + 40, bytesAudio);
}

static void micLiberaSePossivel()
{
  if (micTaskAtiva.load() || micHTTPAtivo.load())
    return;

  if (micLiberando.test_and_set())
    return;

  free(micWav);
  micWav = nullptr;

  micLiberando.clear();
  micOcupado.store(false);
}

static bool micPreparaGravacao()
{
  // Reserva o WAV completo antes de iniciar a captura.
  micWav = (uint8_t *)heap_caps_malloc(
      MIC_TAMANHO_WAV, MALLOC_CAP_8BIT);

  if (!micWav)
  {
    logaM(LOG_CRITICO,
          "Sem memoria para WAV! Maior bloco: %u",
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return false;
  }

  wavHeader(micWav, MIC_TAMANHO_WAV - 44);

  micCancelado.store(false);
  micFinalizado.store(false);
  micTaskAtiva.store(true);

  return true;
}

static bool micGravaWav()
{
  const uint32_t totalAmostras = SAMPLE_RATE * DURACAO_SEG;
  const uint32_t inicio = millis();
  uint32_t capturadas = 0;

  while (capturadas < totalAmostras && !micCancelado.load())
  {
    const uint32_t quantidade =
        std::min<uint32_t>(256, totalAmostras - capturadas);

    size_t bytesLidos = 0;

    esp_err_t err = i2s_read(
        I2S_PORT,
        amostras32,
        quantidade * sizeof(int32_t),
        &bytesLidos,
        pdMS_TO_TICKS(1000));

    if (err != ESP_OK || bytesLidos == 0)
    {
      logaM(LOG_AVISO, "Erro na leitura I2S: %d", err);

      // Completa o restante do arquivo com silencio.
      memset(micWav + 44 + capturadas * sizeof(int16_t),
             0,
             (totalAmostras - capturadas) * sizeof(int16_t));
      break;
    }

    size_t n = std::min<size_t>(
        bytesLidos / sizeof(int32_t), quantidade);

    for (size_t i = 0; i < n; i++)
      amostras16[i] = converteAmostra(amostras32[i]);

    memcpy(
        micWav + 44 + capturadas * sizeof(int16_t),
        amostras16,
        n * sizeof(int16_t));

    capturadas += n;
  }

  uint32_t duracaoMs = millis() - inicio;

  // Publica o audio completo para o callback HTTP.
  micFinalizado.store(true);

  logaM(LOG_NORMAL, "Gravacao finalizada: %lu amostras em %lu ms",
        (unsigned long)capturadas,
        (unsigned long)duracaoMs);

  return capturadas == totalAmostras && !micCancelado.load();
}

static void micCapturaTask(void *)
{
  micGravaWav();

  // Publica o audio para o download HTTP.
  micFinalizado.store(true);
  micTaskAtiva.store(false);
  micLiberaSePossivel();

  vTaskDelete(nullptr);
}

static void micAtivacaoTask(void *)
{
  logaM(LOG_NORMAL, "Ativacao de voz: iniciando gravacao");

  bool completa = micGravaWav();

  if (completa)
  {
    // Envia o WAV diretamente ao eTomadaServer.
    HTTPClient http;
    String url = String("http://") + serverGetIPPortStr() + "/api/mic/audio";

    if (!http.begin(url))
    {
      logaM(LOG_AVISO, "Falha ao iniciar HTTP MIC");
    }
    else
    {
      http.setConnectTimeout(3000);
      http.setTimeout(30000);

      http.addHeader("Content-Type", "audio/wav");
      http.addHeader("X-Device-ID", eTomadaDeviceID());

      int status = http.POST(micWav, MIC_TAMANHO_WAV);

      if (status == 200)
      {
        logaM(LOG_NORMAL,
              "Audio enviado ao server (%u bytes)",
              (unsigned)MIC_TAMANHO_WAV);
      }
      else
      {
        String erro = status < 0
                          ? http.errorToString(status)
                          : http.getString();

        logaM(LOG_AVISO,
              "Falha envio MIC: HTTP %d [%s]",
              status, erro.c_str());
      }

      http.end();
    }
  }
  else
  {
    logaM(LOG_AVISO, "Gravacao incompleta, envio cancelado");
  }

  // O buffer permanece valido ate o termino do POST.
  micTaskAtiva.store(false);
  micLiberaSePossivel();

  vTaskDelete(nullptr);
}

MicAtivacaoResultado micAtivar()
{
  if (!mic.ligado)
    return MicAtivacaoResultado::INDISPONIVEL;

  if (micOcupado.exchange(true))
    return MicAtivacaoResultado::OCUPADO;

  if (!micPreparaGravacao())
  {
    micOcupado.store(false);
    return MicAtivacaoResultado::SEM_MEMORIA;
  }

  if (xTaskCreate(
          micAtivacaoTask,
          "micActivate",
          8192,
          nullptr,
          1,
          nullptr) != pdPASS)
  {
    micTaskAtiva.store(false);
    micLiberaSePossivel();
    return MicAtivacaoResultado::ERRO_TASK;
  }

  return MicAtivacaoResultado::OK;
}

void apiMicRecord(AsyncWebServerRequest *request)
{
  if (!mic.ligado)
  {
    request->send(503, "text/plain", "Microfone indisponivel");
    return;
  }

  if (micOcupado.exchange(true))
  {
    request->send(409, "text/plain", "Microfone ocupado");
    return;
  }

  if (!micPreparaGravacao())
  {
    micOcupado.store(false);
    request->send(503, "text/plain", "Memoria insuficiente");
    return;
  }

  const size_t tamanhoWav = MIC_TAMANHO_WAV;
  micHTTPAtivo.store(true);

  // O HTTP so transmite depois da captura completa.
  AsyncWebServerResponse *response = request->beginResponse(
      "audio/wav",
      tamanhoWav,
      [tamanhoWav](uint8_t *buffer,
                   size_t maxLen,
                   size_t index) -> size_t
      {
        if (!micFinalizado.load())
          return RESPONSE_TRY_AGAIN;

        if (index >= tamanhoWav)
          return 0;

        size_t n = std::min(maxLen, tamanhoWav - index);
        memcpy(buffer, micWav + index, n);

        return n;
      });

  if (xTaskCreate(
          micCapturaTask,
          "micRecord",
          4096,
          nullptr,
          1,
          nullptr) != pdPASS)
  {
    delete response;

    micTaskAtiva.store(false);
    micHTTPAtivo.store(false);
    micLiberaSePossivel();

    request->send(503, "text/plain", "Erro criando task MIC");
    return;
  }

  request->onDisconnect([]()
                        {
    micCancelado.store(true);
    micHTTPAtivo.store(false);
    micLiberaSePossivel(); });

  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Content-Disposition",
                      "attachment; filename=\"comando.wav\"");

  logaM(LOG_NORMAL, "Iniciando gravacao...");

  request->send(response);
}

void apiMicActivate(AsyncWebServerRequest *request)
{
  MicAtivacaoResultado resultado = micAtivar();

  if (resultado == MicAtivacaoResultado::OK)
  {
    request->send(
        202,
        "application/json",
        R"({"ok":true,"msg":"Gravacao iniciada"})");
    return;
  }

  if (resultado == MicAtivacaoResultado::OCUPADO)
  {
    request->send(
        409,
        "application/json",
        R"({"ok":false,"msg":"Microfone ocupado"})");
    return;
  }

  request->send(
      503,
      "application/json",
      R"({"ok":false,"msg":"Microfone indisponivel"})");
}
