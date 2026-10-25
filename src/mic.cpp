#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>

#include "mic.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UMIDIFC", nivel, fmt, ##__VA_ARGS__)

#define SAMPLE_RATE 16000
#define DURACAO_SEG 3
#define I2S_PORT I2S_NUM_0

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

// Apenas 1 por eTomada
static Microfone mic = {.ligado = false};

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
