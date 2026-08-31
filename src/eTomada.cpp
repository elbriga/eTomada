#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

#define ETOMADA_VERSAO "1.3.22"
// 1.3.19 - Rede 10 com log server, mac no mDNS,
// 1.3.20 - endpoint de UPLOAD de Firmware
// 1.3.21 - sensor de corrent com task propria
// 1.3.22 - del discover > usar mDNS

#include "eTomada.h"
#include "mestre.h"
#include "wifi.h"
#include "rele.h"
#include "tipoSensores.h"
#include "sensor.h"
#include "ntp.h"
#include "mutex.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "eventos.h"
#include "agendamentos.h"
#include "regras.h"
#include "util.h"
#include "hardwareProfile.h"
#include "umidificador.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("eTOMADA", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

// Modo de Operação
ModoOperacao modoOperacao = MODO_NO;

String deviceID;
String eTomadaDeviceIDPadrao();
void TESTES(); // testes.cpp

void eTomadaInit0()
{
  mutexInit();

  Preferences prefs;
  prefs.begin("eTomada", false);

  // Para testes
  // prefs.putUChar("modo", MODO_CONTROLADOR);
  // prefs.putString("deviceID", "QUARTO"); // Nao pode ser maior que 32 chars

  if (!prefs.isKey("modo"))
    prefs.putUChar("modo", MODO_NO);
  modoOperacao =
      (prefs.getUChar("modo") == MODO_CONTROLADOR) ? MODO_CONTROLADOR : MODO_NO;

  if (!prefs.isKey("deviceID"))
    prefs.putString("deviceID", eTomadaDeviceIDPadrao());
  deviceID = prefs.getString("deviceID");

  logaM(LOG_NORMAL, "DeviceID: %s", deviceID.c_str());
  logaM(LOG_NORMAL, "Board: %s", hardwareProfile.board);
  logaM(LOG_NORMAL, "Modelo: %s", hardwareProfile.modelo);
  logaM(LOG_NORMAL, "Modo de Operação: %s", eTomadaGetModoOperacaoStr());
  logaM(LOG_NORMAL, "MAC: %s", getMACStr().c_str());

  prefs.end();
}

void eTomadaInit()
{
  TESTES(); // Centraliza os códigos de testes

  mestreInit();

  eventosInit();

  agendamentosInit();

  logaM(LOG_NORMAL, "Inicializando Relés Locais:");
  relesInit();

  logaM(LOG_NORMAL, "Inicializando Sensores Locais:");
  sensoresInit();

  logaM(LOG_NORMAL, "Inicializando Botões Locais:");
  botoesInit();

  if (modoOperacao == MODO_CONTROLADOR) // TODO :: MODO_NO com nodo/recurso remoto?
  {
    logaM(LOG_NORMAL, "Inicializando Nodos Remotos:");
    nodoRemotoInit();

    logaM(LOG_NORMAL, "Inicializando Recursos Remotos:");
    recursosRemotosInit();
  }

  logaM(LOG_NORMAL, "Inicializando Recursos:");
  recursosInit();

  if (modoOperacao == MODO_CONTROLADOR)
  {
    logaM(LOG_NORMAL, "Inicializando Regras:");
    regrasInit();
  }
}

ModoOperacao eTomadaGetModoOperacao()
{
  return modoOperacao;
}

const char *eTomadaGetModoOperacaoStr()
{
  switch (modoOperacao)
  {
  case MODO_CONTROLADOR:
    return "CONTROLADOR";
  case MODO_NO:
    return "NÓ";
  default:
    return "MODOOPERACAOINVALIDO!";
  }
}

String eTomadaDeviceID()
{
  return deviceID;
}

String eTomadaDeviceIDPadrao()
{
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "etomada_" + mac.substring(mac.length() - 6);
}

String eTomadaGetVersao()
{
  return ETOMADA_VERSAO;
}
String eTomadaDeviceModel()
{
  return hardwareProfile.modelo;
}
String eTomadaDeviceBoard()
{
  return hardwareProfile.board;
}

String eTomadaGetSnapshotJSON()
{
  JsonDocument doc;

  doc["device_id"] = eTomadaDeviceID(); // Ex.: QUARTO
  doc["device_model"] = eTomadaDeviceModel();
  doc["device_board"] = eTomadaDeviceBoard();
  doc["fw_version"] = eTomadaGetVersao();

  doc["mac"] = getMACStr();
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["wifiPower"] = WiFi.RSSI(); // TODO :: mostar na interface

  if (umidificadorAtivo())
    doc["umidPower"] = umidificadorGetEstado();

  doc["uptime"] = millis();
  time_t now = 0;
  time(&now);
  doc["timestamp"] = (unsigned long)now;

  time_t agora = time(nullptr);
  doc["datahora"] = (unsigned long)agora;

  // TODO :: remover!
  struct tm timeinfo;
  sysGetTime(&timeinfo);
  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  doc["datahorastr"] = formattedTime;

  // TODO :: Enviar estes dados no RECURSO_SENSOR, mesmo que duplicados, assim o sensor fica auto-suficiente e pode ser mostrado no nodo pai
  {
    TipoSensor *ts;
    int totTS = tipoSensorGetCount();
    JsonArray tipoSensores = doc["tipoSensores"].to<JsonArray>();
    for (int i = 0; i < totTS; i++)
    {
      ts = tipoSensorGetPorIndice(i);
      if (!ts)
        continue;

      tipoSensores.add(tipoSensorGetJSONDoc(ts));
    }
  }

  Recurso *recurso;
  int totRecursos = recursosGetCount();
  JsonArray recursos = doc["recursos"].to<JsonArray>();
  for (int i = 0; i < totRecursos; i++)
  {
    recurso = recursoGetPorIndice(i);
    if (!recurso)
      continue;

    recursos.add(recursoGetJSONDoc(recurso));
  }

  JsonDocument regrasJS;
  regrasGetJSONDoc(regrasJS);
  doc["regras"] = regrasJS;

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaRoleta()
{
  logaTitulo("ROLETA!");

  int totRelesLocais = 0;
  int totRecursos = recursosGetCount();
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *recurso = recursoGetPorIndice(r);
    if (recurso->tipo == RECURSO_RELE && !recurso->remoto)
    {
      totRelesLocais++;
    }
  }

  Recurso **relesLocais = (Recurso **)calloc(sizeof(Recurso *), totRelesLocais);
  if (!relesLocais)
  {
    logaTitulo("ROLETA :: ERRO DE MALLOC");
    return;
  }

  int rli = 0;
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *recurso = recursoGetPorIndice(r);
    if (recurso->tipo == RECURSO_RELE && !recurso->remoto)
    {
      relesLocais[rli++] = recurso;
    }
  }

  for (int r = 0; r < totRelesLocais; r++)
  {
    Recurso *recurso = relesLocais[r];
    recursoSet(recurso, "OFF");
  }

  int delay = 25, delta = 2;
  int num = esp_random() % totRelesLocais;
  int oldNum = num;
  int loop = 0;

  while (delay < 440)
  {
    esp_task_wdt_reset(); // alimenta o watchdog

    oldNum = num;
    num++;
    if (num >= totRelesLocais)
    {
      num = 0;
    }
    recursoSet(relesLocais[oldNum], "OFF");
    recursoSet(relesLocais[num], "ON");

    loop++;
    if (loop > 40)
    {
      delay += delta;
      if (loop > 90)
      {
        delta += 1;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(delay));
  }

  free(relesLocais);

  logaM(LOG_AVISO, "** Numero Sorteado: %d **", num + 1);
}

void eTomadaFactoryReset()
{
  MutexLock lockPrefs(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lockPrefs)
  {
    logaM(LOG_CRITICO, "Erro de mutex no factory reset!");
    return;
  }

  Preferences prefs;

  prefs.begin("reles", false);
  prefs.clear();
  prefs.end();

  prefs.begin("sensores", false);
  prefs.clear();
  prefs.end();

  prefs.begin("recursosRemotos", false);
  prefs.clear();
  prefs.end();

  logaTitulo("RESET!");
  ESP.restart();
}

String getMACStr()
{
  return WiFi.macAddress();
}
