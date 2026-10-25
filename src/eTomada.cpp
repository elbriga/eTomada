#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

#define ETOMADA_VERSAO "4.0.2"
// 1.3.19 - Rede 10 com log server, mac no mDNS,
// 1.3.20 - endpoint de UPLOAD de Firmware
// 1.3.21 - sensor de corrent com task propria
// 1.3.22 - del discover > usar mDNS
// 1.3.23 - Removido override dos reles e OTA automatico
// 1.3.24 - btn reset
// 2.0.0  - recovery.cpp
// 2.0.1  - sensor de Chuva!
// 2.0.2  - umidificador com ventilador
// 2.0.3  - umidificador remoto e refactor del mestre.online
// 2.0.4  - Interface de gerenciamento de Nodos e Recursos
// 2.0.5  - Nomes Locks
// 2.0.6  - HTTP Auth + Limite de tentativas
// 3.0.0  - Del mDNS! Usar DNS .etomada
// 3.0.1  - Bundle index.html
// 3.0.2  - Para tudo durante OTA
// 3.0.3  - File Manager
// 3.0.4  - DNS Reverso
// 3.0.5  - Add comando TOGGLEF para o EKAZA
// 4.0.0  - DEL mestre > Ave SERVER!
// 4.0.1  - Add recurso MIC

#include "eTomada.h"
#include "server.h"
#include "wifi.h"
#include "rele.h"
#include "tipoSensores.h"
#include "sensor.h"
#include "ntp.h"
#include "mutex.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "eventos.h"
#include "agendamentos.h"
#include "regras.h"
#include "util.h"
#include "hardwareProfile.h"
#include "sensorChuva.h"
#include "mic.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("eTOMADA", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

String deviceID;
String eTomadaDeviceIDPadrao();
void TESTES(); // testes.cpp

void eTomadaInit0()
{
  mutexInit();

  Preferences prefs;
  prefs.begin("eTomada", false);

  // Para testes
  // prefs.putString("deviceID", "QUARTO"); // Nao pode ser maior que 32 chars

  if (!prefs.isKey("deviceID"))
    prefs.putString("deviceID", eTomadaDeviceIDPadrao());

  deviceID = prefs.getString("deviceID");

  logaM(LOG_NORMAL, "DeviceID: %s", deviceID.c_str());
  logaM(LOG_NORMAL, "Board: %s", hardwareProfile.board);
  logaM(LOG_NORMAL, "Modelo: %s", hardwareProfile.modelo);
  logaM(LOG_NORMAL, "MAC: %s", getMACStr().c_str());

  prefs.end();
}

void eTomadaInit()
{
  TESTES(); // Centraliza os códigos de testes

  eventosInit();

  agendamentosInit();

  relesInit();
  sensoresInit();
  botoesInit();
  micInit();

  eTomadaLoadConfig();
}

static int configLoadCount = 0;
void eTomadaLoadConfig()
{
  MutexLock configLock(configMutex, "eTomadaLoadConfig config");
  if (!configLock)
  {
    logaM(LOG_CRITICO, "eTomadaLoadConfig: CONFIG mutex timeout!");
    return;
  }

  {
    MutexLock lock(modeloMutex, "eTomadaLoadConfig modelo");
    if (!lock)
    {
      logaM(LOG_CRITICO, "eTomadaLoadConfig: mutex timeout");
      return;
    }

    logaM(LOG_AVISO, ">>> eTomadaLoadConfig[%d]", configLoadCount);

    logaM(LOG_NORMAL, "%sInicializando Recursos:", configLoadCount ? "(RE)" : "");
    recursosInit();

    logaM(LOG_NORMAL, "%sInicializando Regras:", configLoadCount ? "(RE)" : "");
    regrasInit();

    // Drivers
    sensorChuvaInit();

    configLoadCount++;
  }

  // Regras Boot fora do Lock de reursos, mas dentro do lock de config
  regrasBoot();
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

  {
    MutexLock lock(modeloMutex, "eTomadaGetSnapshotJSON");
    if (lock)
    {
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
      doc["rules_revision"] = regrasGetRevision();

      if (sensorChuvaAtivo())
        doc["horasSemChuva"] = sensorChuvaGetHorasSemChuva();
    }
    else
    {
      logaM(LOG_CRITICO, "getSnapshot :: Erro de lock!");
      JsonArray recursosZerados = doc["recursos"].to<JsonArray>();
      JsonArray regrasZeradas = doc["regras"].to<JsonArray>();
    }
  }

  doc["device_id"] = eTomadaDeviceID(); // Ex.: QUARTO
  doc["device_model"] = eTomadaDeviceModel();
  doc["device_board"] = eTomadaDeviceBoard();
  doc["fw_version"] = eTomadaGetVersao();
  doc["api"] = "Full";

  doc["mac"] = getMACStr();
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["wifiPower"] = WiFi.RSSI(); // TODO :: mostar na interface
  doc["uptime"] = millis();

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaFactoryReset()
{
  {
    MutexLock lockPrefs(configMutex, "eTomadaFactoryReset config");
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
  }

  // TODO :: Arquivo.json!

  logaTitulo("RESET!");
  ESP.restart();
}

String getMACStr()
{
  return WiFi.macAddress();
}
