#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "umidificador.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "agendamentos.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UMIDIFC", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static UmidificadorEstado estadoAtual = UMID_DESLIGADO;
static UmidificadorFanEstado estadoFanAtual = UMIDFAN_DESLIGADO;

void umidificadorSetEstadoTask(void *args);
void umidSendClick(int pin);

void umidificadorInit()
{
  if (!umidificadorAtivo())
    return;

  Preferences prefs;
  prefs.begin("eTomada", false);

  if (!prefs.isKey("umidPower"))
    prefs.putUChar("umidPower", UMID_POWER3); // Default
  if (!prefs.isKey("umidFanPower"))
    prefs.putUChar("umidFanPower", UMIDFAN_DESLIGADO); // Default

  UmidificadorEstado ultimoEstado = (UmidificadorEstado)prefs.getUChar("umidPower");
  if (ultimoEstado < UMID_DESLIGADO || ultimoEstado > UMID_POWER5)
    ultimoEstado = UMID_POWER3; // Default

  prefs.end();

  pinMode(hardwareProfile.umidificador.onPin, OUTPUT);
  pinMode(hardwareProfile.umidificador.umidPin, OUTPUT);

  digitalWrite(hardwareProfile.umidificador.onPin, LOW);
  digitalWrite(hardwareProfile.umidificador.umidPin, LOW);

  if (umidificadorFanAtivo())
  {
    pinMode(hardwareProfile.umidificador.fanPin, OUTPUT);
    digitalWrite(hardwareProfile.umidificador.fanPin, LOW);
  }

  logaM(LOG_NORMAL, "Umidificador Encontrado! Ligar em Power [%d]", ultimoEstado);
  umidificadorSetEstado(ultimoEstado);
}

bool umidificadorAtivo()
{
  return hardwareProfile.umidificador.onPin != 255 && hardwareProfile.umidificador.umidPin != 255;
}

bool umidificadorFanAtivo()
{
  return hardwareProfile.umidificador.fanPin != 255;
}

UmidificadorEstado umidificadorGetEstado()
{
  return estadoAtual;
}

UmidificadorFanEstado umidificadorFanGetEstado()
{
  return estadoFanAtual;
}

bool umidificadorFanSetEstado(UmidificadorFanEstado estado)
{
  if (!umidificadorFanAtivo())
  {
    logaM(LOG_AVISO, "umidificadorFanSetEstado sem Umidificador FAN??");
    return false;
  }

  if (estado < UMIDFAN_DESLIGADO || estado > UMIDFAN_POWER3)
  {
    logaM(LOG_AVISO, "umidificadorFanSetEstado Abortando estado invalido [%d]!", estado);
    return false;
  }

  estadoFanAtual = estado;

  // Deve chamar umidificadorSetEstado() depois que le estadoFanAtual
  return true;
}

static volatile bool umidTaskRodando = false;
void umidificadorSetEstado(UmidificadorEstado estado)
{
  if (!umidificadorAtivo())
  {
    logaM(LOG_AVISO, "umidificadorSetEstado sem Umidificador??");
    return;
  }

  if (umidTaskRodando)
  {
    logaM(LOG_AVISO, "umidificadorSetEstado Abortando Task Dupla!");
    return;
  }

  if (estado < UMID_DESLIGADO || estado > UMID_POWER5)
  {
    logaM(LOG_AVISO, "umidificadorSetEstado Abortando estado invalido [%d]!", estado);
    return;
  }

  umidTaskRodando = true;

  estadoAtual = estado;

  char msgFan[20] = {0};
  if (umidificadorFanAtivo())
    sprintf(msgFan, "[fan:%d]", estadoFanAtual);
  logaM(LOG_NORMAL, "Umidificador > Ligar em Power [%d]%s", estadoAtual, msgFan);

  if (xTaskCreate(
          umidificadorSetEstadoTask,
          "umidSet",
          4096,
          nullptr,
          1,
          nullptr) != pdPASS)
  {
    umidTaskRodando = false;
    logaM(LOG_CRITICO, "Falha ao criar task umidSet");
  }
}

void umidificadorSetEstadoTask(void *args)
{
  // Salvar ultimo estado
  Preferences prefs;
  prefs.begin("eTomada", false);
  if (prefs.getUChar("umidPower") != estadoAtual)
    prefs.putUChar("umidPower", estadoAtual);
  if (prefs.getUChar("umidFanPower") != estadoFanAtual)
    prefs.putUChar("umidFanPower", estadoFanAtual);
  prefs.end();

  // TODO :: mudar estado sem desligar

  // Desligar
  digitalWrite(hardwareProfile.umidificador.onPin, LOW);
  digitalWrite(hardwareProfile.umidificador.umidPin, LOW);
  if (umidificadorFanAtivo())
    digitalWrite(hardwareProfile.umidificador.fanPin, LOW);

  if (estadoAtual > UMID_DESLIGADO || (umidificadorFanAtivo() && estadoFanAtual > UMIDFAN_DESLIGADO))
  {
    // Ligar
    vTaskDelay(pdMS_TO_TICKS(100));
    logaM(LOG_NORMAL, "Ligando Umidificador");
    digitalWrite(hardwareProfile.umidificador.onPin, HIGH);
    // Aguardar o "boot"
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
  else
    logaM(LOG_NORMAL, "Desligando Umidificador");

  if (estadoAtual > UMID_DESLIGADO)
  {
    // Enviar [1-3] clicks no botao de nevoa
    for (int i = 0; i < estadoAtual; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(1500));

      umidSendClick(hardwareProfile.umidificador.umidPin);
      logaM(LOG_NORMAL, "Click!");
    }

    logaM(LOG_NORMAL, "Umidificador ligado no POWER[%d]", (int)estadoAtual);

    int minutosOff = (4 - estadoAtual) * 30; // timer de 30, 60 ou 90 minutos, conforme o power
    agendamentosLimpa(AGEND_RECURSO, "UMIDIFICADOR");
    agendamentosAdd(AGEND_RECURSO, minutosOff * 60 * 1000, "UMIDIFICADOR", 0);
    logaM(LOG_NORMAL, "Agendado desligamento para daqui [%d] minutos", minutosOff);
  }

  if (umidificadorFanAtivo() && estadoFanAtual > UMIDFAN_DESLIGADO)
  {
    // Enviar [1-3] clicks no botao do ventilador
    for (int i = 0; i < estadoFanAtual; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(1500));

      umidSendClick(hardwareProfile.umidificador.fanPin);
      logaM(LOG_NORMAL, "Click Fan!");
    }

    logaM(LOG_NORMAL, "Umidificador FAN ligado no POWER[%d]", (int)estadoFanAtual);
  }

  umidTaskRodando = false;
  vTaskDelete(NULL);
}

void umidSendClick(int pin)
{
  digitalWrite(pin, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));

  digitalWrite(pin, HIGH);
  vTaskDelay(pdMS_TO_TICKS(250));

  digitalWrite(pin, LOW);
  vTaskDelay(pdMS_TO_TICKS(10));
}

String umidificadorSetFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("umidificadorSetFromJSON", doc, json))
    return "JSON Invalido";

  int novoEstado = !doc["estado"].isNull() ? doc["estado"].as<int>() : -1;
  int novoEstadoFan = !doc["estadoFan"].isNull() ? doc["estadoFan"].as<int>() : -1;
  doc.clear();

  UmidificadorEstado estadoFinal = estadoAtual;
  if (novoEstado != -1)
  {
    if (novoEstado < 0 || novoEstado > UMID_POWER5)
      return "Estado Invalido";
    else
      estadoFinal = (UmidificadorEstado)novoEstado;
  }

  bool setFanOK = false;
  if (novoEstadoFan != -1)
    setFanOK = umidificadorFanSetEstado((UmidificadorFanEstado)novoEstadoFan);

  if (novoEstado != -1 || setFanOK)
    umidificadorSetEstado(estadoFinal);

  return "OK";
}

JsonDocument umidificadorGetJSONDoc()
{
  JsonDocument doc;

  doc["num"] = 1;
  doc["estado"] = umidificadorGetEstado();

  if (umidificadorFanAtivo())
    doc["estadoFan"] = umidificadorFanGetEstado();

  return doc;
}
