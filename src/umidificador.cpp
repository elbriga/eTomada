#include <Arduino.h>
#include <Preferences.h>

#include "umidificador.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "agendamentos.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UMIDIFC", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static UmidificadorEstado estadoAtual = UMID_DESLIGADO;

void umidificadorSetEstadoTask(void *args);
void umidSendClick();

void umidificadorInit()
{
  if (!umidificadorAtivo())
    return;

  Preferences prefs;
  prefs.begin("eTomada", false);

  if (!prefs.isKey("umidPower"))
    prefs.putUChar("umidPower", UMID_POWER3); // Default

  UmidificadorEstado ultimoEstado = (UmidificadorEstado)prefs.getUChar("umidPower");
  if (ultimoEstado < UMID_DESLIGADO || ultimoEstado > UMID_POWER5)
    ultimoEstado = UMID_POWER3; // Default

  prefs.end();

  pinMode(hardwareProfile.umidificador[0], OUTPUT);
  pinMode(hardwareProfile.umidificador[1], OUTPUT);

  digitalWrite(hardwareProfile.umidificador[0], LOW);
  digitalWrite(hardwareProfile.umidificador[1], LOW);

  logaM(LOG_NORMAL, "Umidificador Encontrado! Ligar em Power [%d]", ultimoEstado);
  umidificadorSetEstado(ultimoEstado);
}

bool umidificadorAtivo()
{
  return hardwareProfile.umidificador[0] > 0 && hardwareProfile.umidificador[1] > 0;
}

UmidificadorEstado umidificadorGetEstado()
{
  return estadoAtual;
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

  umidTaskRodando = true;

  estadoAtual = estado;

  if (xTaskCreate(
          umidificadorSetEstadoTask,
          "umidSet",
          4096,
          (void *)(intptr_t)estado,
          1,
          NULL) != pdPASS)
  {
    umidTaskRodando = false;
    logaM(LOG_CRITICO, "Falha ao criar task umidSet");
  }
}

void umidificadorSetEstadoTask(void *args)
{
  UmidificadorEstado estado = (UmidificadorEstado)(intptr_t)args;

  if (estado)
  {
    // Salvar ultimo estado
    Preferences prefs;
    prefs.begin("eTomada", false);
    prefs.putUChar("umidPower", estado);
    prefs.end();
  }

  // TODO :: mudar estado sem desligar

  // Desligar
  digitalWrite(hardwareProfile.umidificador[0], LOW);
  digitalWrite(hardwareProfile.umidificador[1], LOW);

  if (estado == UMID_DESLIGADO)
  {
    logaM(LOG_NORMAL, "Desligando Umidificador");
    umidTaskRodando = false;
    vTaskDelete(NULL);
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  // Ligar
  logaM(LOG_NORMAL, "Ligando Umidificador");
  digitalWrite(hardwareProfile.umidificador[0], HIGH);
  // Aguardar o "boot"
  vTaskDelay(pdMS_TO_TICKS(1500));

  // Enviar [1-3] clicks no botao de nevoa
  for (int i = 0; i < estado; i++)
  {
    vTaskDelay(pdMS_TO_TICKS(1500));

    umidSendClick();
    logaM(LOG_NORMAL, "Click!");
  }

  logaM(LOG_NORMAL, "Umidificador ligado no POWER[%d]", (int)estado);

  umidTaskRodando = false;
  vTaskDelete(NULL);
}

void umidSendClick()
{
  digitalWrite(hardwareProfile.umidificador[1], LOW);
  vTaskDelay(pdMS_TO_TICKS(50));

  digitalWrite(hardwareProfile.umidificador[1], HIGH);
  vTaskDelay(pdMS_TO_TICKS(250));

  digitalWrite(hardwareProfile.umidificador[1], LOW);
  vTaskDelay(pdMS_TO_TICKS(10));
}

String umidificadorSetFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err)
    return "JSON Invalido";

  int estado = doc["estado"].as<int>();
  doc.clear();

  if (estado < 0 || estado > UMID_POWER5)
    return "Estado Invalido";

  umidificadorSetEstado((UmidificadorEstado)estado);

  return "OK";
}
