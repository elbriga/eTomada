#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "umidificador.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "agendamentos.h"
#include "util.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UMIDIFC", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

// Apenas 1 por eTomada
static Umidificador umid = {
    .estado = UMID_DESLIGADO,
    .estadoFan = UMIDFAN_DESLIGADO,
};

void umidificadorSetEstadoTask(void *args);
void umidSendClick(int pin);

Umidificador *umidificadorGet()
{
  return &umid;
}

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
    ultimoEstado = UMID_POWER1; // Default

  UmidificadorFanEstado ultimoEstadoFan = (UmidificadorFanEstado)prefs.getUChar("umidFanPower");
  if (ultimoEstadoFan < UMIDFAN_DESLIGADO || ultimoEstadoFan > UMIDFAN_POWER3)
    ultimoEstadoFan = UMIDFAN_DESLIGADO; // Default

  prefs.end();

  pinMode(hardwareProfile.umidificador.onPin, OUTPUT);
  pinMode(hardwareProfile.umidificador.umidPin, OUTPUT);

  digitalWrite(hardwareProfile.umidificador.onPin, LOW);
  digitalWrite(hardwareProfile.umidificador.umidPin, LOW);

  if (umidificadorFanAtivo())
  {
    pinMode(hardwareProfile.umidificador.fanPin, OUTPUT);
    digitalWrite(hardwareProfile.umidificador.fanPin, LOW);

    umidificadorFanSetEstado(ultimoEstadoFan);
  }

  String msgInit = umidificadorSetEstado(ultimoEstado);
  logaM(LOG_NORMAL, "Umidificador Encontrado! [%s]", msgInit.c_str());
}

bool umidificadorAtivo()
{
  return hardwareProfile.umidificador.onPin != 255 && hardwareProfile.umidificador.umidPin != 255;
}

bool umidificadorFanAtivo()
{
  return hardwareProfile.umidificador.fanPin != 255;
}

String umidificadorFanSetEstado(UmidificadorFanEstado estado)
{
  if (!umidificadorFanAtivo())
    return "umidificadorFanSetEstado sem Umidificador FAN??";

  if (estado < UMIDFAN_DESLIGADO || estado > UMIDFAN_POWER3)
    return "umidificadorFanSetEstado Abortando estado invalido";

  umid.estadoFan = estado;

  // Deve chamar umidificadorSetEstado() depois que le estadoFan
  return "OK";
}

static volatile bool umidTaskRodando = false;
String umidificadorSetEstado(UmidificadorEstado estado)
{
  if (!umidificadorAtivo())
    return "umidificadorSetEstado sem Umidificador??";

  if (umidTaskRodando)
    return "umidificadorSetEstado Abortando Task Dupla!";

  if (estado < UMID_DESLIGADO || estado > UMID_POWER5)
    return "umidificadorSetEstado Abortando estado invalido";

  umidTaskRodando = true;

  umid.estado = estado;

  char msgFan[20] = {0};
  if (umidificadorFanAtivo())
    sprintf(msgFan, "[fan:%d]", umid.estadoFan);
  String msg = "Umidificador > Ligar em Power [" + String(umid.estado) + "]" + msgFan;
  logaM(LOG_NORMAL, "%s", msg.c_str());

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

  return msg;
}

void umidificadorSetEstadoTask(void *args)
{
  // Salvar ultimo estado
  Preferences prefs;
  prefs.begin("eTomada", false);
  if (prefs.getUChar("umidPower") != umid.estado)
    prefs.putUChar("umidPower", umid.estado);
  if (prefs.getUChar("umidFanPower") != umid.estadoFan)
    prefs.putUChar("umidFanPower", umid.estadoFan);
  prefs.end();

  // TODO :: mudar estado sem desligar

  // Desligar
  digitalWrite(hardwareProfile.umidificador.onPin, LOW);
  digitalWrite(hardwareProfile.umidificador.umidPin, LOW);
  if (umidificadorFanAtivo())
    digitalWrite(hardwareProfile.umidificador.fanPin, LOW);

  if (umid.estado > UMID_DESLIGADO || (umidificadorFanAtivo() && umid.estadoFan > UMIDFAN_DESLIGADO))
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

  if (umid.estado > UMID_DESLIGADO)
  {
    // Enviar [1-3] clicks no botao de nevoa
    for (int i = 0; i < umid.estado; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(1500));

      umidSendClick(hardwareProfile.umidificador.umidPin);
      logaM(LOG_NORMAL, "Click!");
    }

    logaM(LOG_NORMAL, "Umidificador ligado no POWER[%d]", (int)umid.estado);

    int minutosOff = (4 - umid.estado) * 30; // timer de 30, 60 ou 90 minutos, conforme o power
    agendamentosLimpa(AGEND_RECURSO, "UMIDIFICADOR");
    agendamentosAdd(AGEND_RECURSO, minutosOff * 60 * 1000, "UMIDIFICADOR", 0);
    logaM(LOG_NORMAL, "Agendado desligamento para daqui [%d] minutos", minutosOff);
  }

  if (umidificadorFanAtivo() && umid.estadoFan > UMIDFAN_DESLIGADO)
  {
    // Enviar [1-3] clicks no botao do ventilador
    for (int i = 0; i < umid.estadoFan; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(1500));

      umidSendClick(hardwareProfile.umidificador.fanPin);
      logaM(LOG_NORMAL, "Click Fan!");
    }

    logaM(LOG_NORMAL, "Umidificador FAN ligado no POWER[%d]", (int)umid.estadoFan);
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

  UmidificadorEstado estadoFinal = umid.estado;
  if (novoEstado != -1)
  {
    if (novoEstado < 0 || novoEstado > UMID_POWER5)
      return "Estado Invalido";
    else
      estadoFinal = (UmidificadorEstado)novoEstado;
  }

  String setFanMsg = "";
  if (novoEstadoFan != -1)
  {
    // Aqui só muda a variavel de controle
    setFanMsg = umidificadorFanSetEstado((UmidificadorFanEstado)novoEstadoFan);
    if (setFanMsg != "OK")
      logaM(LOG_CRITICO, "umidSetFromJson FAN > %s", setFanMsg.c_str());
  }

  String ret = "Sem Alteração";
  if (novoEstado != -1 || setFanMsg == "OK")
    // Aqui faz o acionamento do Umid e do Fan
    ret = umidificadorSetEstado(estadoFinal);

  return ret;
}

JsonDocument umidificadorGetJSONDoc(Recurso *r, bool full)
{
  JsonDocument doc;

  Umidificador *u = recursoGetUmidificador(r);
  if (!u)
    return doc;

  doc["estado"] = u->estado;
  doc["estadoFan"] = u->estadoFan;

  return doc;
}
