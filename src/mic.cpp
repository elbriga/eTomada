#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <algorithm>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

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
#define MIC_FIFO_BYTES 8192

static StreamBufferHandle_t micFila = nullptr;

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

  micFila = xStreamBufferCreate(MIC_FIFO_BYTES, 1);
  if (!micFila)
  {
    logaM(LOG_CRITICO, "Erro criando buffer do microfone");
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
  if (!micTaskAtiva.load() && !micHTTPAtivo.load())
    micOcupado.store(false);
}

// Captura o audio fora da task do AsyncTCP.
static void micCapturaTask(void *)
{
  const uint32_t totalAmostras = SAMPLE_RATE * DURACAO_SEG;
  const uint32_t inicio = millis();

  uint32_t capturadas = 0;
  bool erroI2S = false;

  while (capturadas < totalAmostras && !micCancelado.load())
  {
    const uint32_t quantidade =
        std::min<uint32_t>(256, totalAmostras - capturadas);

    size_t bytesLidos = 0;
    size_t n = 0;

    if (!erroI2S)
    {
      esp_err_t err = i2s_read(
          I2S_PORT,
          amostras32,
          quantidade * sizeof(int32_t),
          &bytesLidos,
          pdMS_TO_TICKS(1000));

      if (err != ESP_OK || bytesLidos == 0)
      {
        logaM(LOG_AVISO, "Erro na leitura I2S: %d", err);
        erroI2S = true;
      }
      else
      {
        n = bytesLidos / sizeof(int32_t);

        for (size_t i = 0; i < n; i++)
          amostras16[i] = converteAmostra(amostras32[i]);
      }
    }

    // Em caso de erro, completa o WAV com silencio.
    if (erroI2S)
    {
      n = quantidade;
      memset(amostras16, 0, n * sizeof(int16_t));
    }

    const size_t totalBytes = n * sizeof(int16_t);
    size_t enviados = 0;

    // FIFO absorve diferencas de velocidade entre I2S e HTTP.
    while (enviados < totalBytes && !micCancelado.load())
    {
      size_t bytes = xStreamBufferSend(
          micFila,
          (uint8_t *)amostras16 + enviados,
          totalBytes - enviados,
          pdMS_TO_TICKS(100));

      enviados += bytes;

      if (millis() - inicio > 15000)
      {
        logaM(LOG_AVISO, "Timeout no envio do audio");
        break;
      }
    }

    if (enviados != totalBytes)
      break;

    capturadas += n;
  }

  logaM(LOG_NORMAL, "Gravacao finalizada: %lu amostras",
        (unsigned long)capturadas);

  micFinalizado.store(true);
  micTaskAtiva.store(false);
  micLiberaSePossivel();

  vTaskDelete(nullptr);
}

void apiMicRecord(AsyncWebServerRequest *request)
{
  if (!mic.ligado || !micFila)
  {
    request->send(503, "text/plain", "Microfone indisponivel");
    return;
  }

  if (micOcupado.exchange(true))
  {
    request->send(409, "text/plain", "Microfone ocupado");
    return;
  }

  const uint32_t totalAmostras = SAMPLE_RATE * DURACAO_SEG;
  const uint32_t bytesAudio = totalAmostras * sizeof(int16_t);
  const size_t tamanhoWav = 44 + bytesAudio;

  xStreamBufferReset(micFila);

  micCancelado.store(false);
  micFinalizado.store(false);
  micTaskAtiva.store(true);
  micHTTPAtivo.store(true);

  // O callback HTTP apenas consome os bytes ja capturados.
  AsyncWebServerResponse *response = request->beginResponse(
      "audio/wav",
      tamanhoWav,
      [bytesAudio, tamanhoWav](uint8_t *buffer,
                               size_t maxLen,
                               size_t index) -> size_t
      {
        if (index >= tamanhoWav)
          return 0;

        if (index < 44)
        {
          uint8_t header[44];
          wavHeader(header, bytesAudio);

          size_t n = std::min(maxLen, size_t(44) - index);
          memcpy(buffer, header + index, n);
          return n;
        }

        size_t n = xStreamBufferReceive(
            micFila,
            buffer,
            std::min(maxLen, tamanhoWav - index),
            0);

        if (n > 0)
          return n;

        // Captura encerrada: preserva o tamanho esperado do WAV.
        if (micFinalizado.load())
        {
          n = std::min(maxLen, tamanhoWav - index);
          memset(buffer, 0, n);
          return n;
        }

        // Ainda nao ha amostras; tentar novamente sem bloquear.
        return RESPONSE_TRY_AGAIN;
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
    micOcupado.store(false);

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
