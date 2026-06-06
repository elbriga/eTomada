#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "reles.h"
#include "regras.h"
#include "tipoSensores.h"
#include "sensores.h"
#include "ntp.h"
#include "mutex.h"
#include "loga.h"
#include "prefs.h"

// Valores default
static Rele relesConfigDefault[MAX_RELES] = {
  { 1, 16, "Luz",             "OF|02:00|07:59", 1, 0 },
  { 2, 13, "Umidificador",    "ON|08:00|20:00", 1, 0 },
  { 3, 17, "Ventilador",      "SE|S1>20|S1<10", 1, 0 },
  { 4, 14, "Desumidificador", "",               1, 0 },
  { 5, -1, "", "", 0, 0 },
  { 6, -1, "", "", 0, 0 },
  { 7, -1, "", "", 0, 0 },
  { 8, -1, "", "", 0, 0 }
};

static Sensor sensoresConfigDefault[MAX_SENSORES] = {
  { 1,  1, "Temp", "AHT10t",   0, 0 },
  { 2,  2, "Umid", "UmidXPTO", 0, 0 },
  { 3,  3, "lux",  "LUXXPTO",  0, 0 },
  { 4, -1, "",     "",         0, 0 }
};

// Whitelist de pinos
int pinosOutOK[] = { 0, 2, 3, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 };
int pinosInOK[] = { 0, 1, 2, 3 };

void eTomadaInit() {
  mutexInit();
  
  logaMensagem("Carregando Configuracao dos reles:");
  relesInit(); // Carrega os reles do prefs

  logaMensagem("Carregando Configuracao dos Sensores:");
  sensoresInit(); // Carrega os sensores do prefs

  Serial.println("");
}

String eTomadaGetSnapshotJSON() {
  JsonDocument doc;
  
  uint64_t MAC = ESP.getEfuseMac();
  char deviceID[32];
  
  sprintf(deviceID, "etomada_%04X", (uint16_t)(MAC & 0xFFFF));
  doc["device_id"]   = deviceID;
  doc["device_name"] = "eTomada Sala"; // TODO
  doc["fw_version"]  = "1.3.0";
  
  char macStr[18];
  sprintf(
    macStr,
    "%02X:%02X:%02X:%02X:%02X:%02X",
    (uint8_t)(MAC >> 40),
    (uint8_t)(MAC >> 32),
    (uint8_t)(MAC >> 24),
    (uint8_t)(MAC >> 16),
    (uint8_t)(MAC >> 8),
    (uint8_t)(MAC)
  );
  doc["mac"] = macStr;
  
  doc["api"]    = 3; // versão da API
  doc["uptime"] = millis();
    
  time_t agora;
  struct tm timeinfo;
  ntpGetTime(&timeinfo, &agora);
  
  doc["datahora"] = (unsigned long)agora;
  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  doc["datahorastr"] = formattedTime;

  Rele *rele;
  int totReles = relesGetCount();
  JsonArray reles = doc["reles"].to<JsonArray>();

  {
    MutexLock lock(releMutex, pdMS_TO_TICKS(2500));

    if (!lock) {
      doc["erro"] = "mutex rele timeout";
    } else {
      for (int i = 1; i <= totReles; i++) {
        rele = releGet(i);
        if (!rele) {
          // TODO :: o que fazer aqui??
          continue;
        }

        reles.add(releGetJSONDoc(rele));
      }
    }
  }

  Sensor *sensor;
  int totSensores = sensoresGetCount();
  JsonArray sensores = doc["sensores"].to<JsonArray>();
  {
    MutexLock lock(sensorMutex, pdMS_TO_TICKS(2500));

    if (!lock) {
      doc["erro"] = "mutex sensor timeout";
    } else {
      for (int i = 1; i <= totSensores; i++) {
        sensor = sensorGet(i);
        if (!sensor) {
          // TODO :: o que fazer aqui??
          continue;
        }

        sensores.add(sensorGetJSONDoc(sensor));
      }
    }
  }
  
  TipoSensor *ts;
  int totTS = tipoSensorGetCount();
  JsonArray tipoSensores = doc["tipoSensores"].to<JsonArray>();
  for (int i=0; i < totTS; i++) {
    ts = tipoSensorGetPorId(i);
    if (!ts) continue;

    tipoSensores.add(tipoSensorGetJSONDoc(ts));
  }

  String out;
  serializeJson(doc, out);

  return out;
}

String eTomadaGetReleString(int numRele) {
  JsonDocument doc;
  {
    MutexLock lock(releMutex, pdMS_TO_TICKS(2500));
    if (!lock) {
      doc["erro"] = "mutex rele timeout";
    } else {
      Rele *rele = releGet(numRele);
      if (!rele) {
        doc["erro"] = "rele invalido";
      } else {
        doc = releGetJSONDoc(rele);
      }
    }
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String eTomadaGetRelesString() {
  JsonDocument doc;
  Rele *rele;
  int totReles = relesGetCount();
  JsonArray reles = doc["reles"].to<JsonArray>();

  {
    MutexLock lock(releMutex, pdMS_TO_TICKS(2500));

    if (!lock) {
      doc["erro"] = "mutex rele timeout";
    } else {
      for (int i = 1; i <= totReles; i++) {
        rele = releGet(i);
        if (!rele) {
          // TODO :: o que fazer aqui??
          continue;
        }

        reles.add(releGetJSONDoc(rele));
      }
    }
  }

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaSalvaReleUnsafe(Rele *rele) {
  Preferences prefs;
  
  prefs.begin("reles", false);
  
  setPrefsAtr(prefs, rele->num, "nome",  String(rele->nome));
  setPrefsAtr(prefs, rele->num, "regra", String(rele->regra));
  setPrefsAtr(prefs, rele->num, "ativo", String(rele->ativo));

  int oldPin = atoi(setPrefsAtr(prefs, rele->num, "pino",  String(rele->pino)).c_str());
  if (rele->pino != oldPin) {
    // Desligar pino antigo
    if (oldPin != -1) {
      digitalWrite(oldPin, LOW);
    }

    // Ativar o pino novo
    if (rele->pino != -1) {
      pinMode(rele->pino, OUTPUT);
    }
  }
  
  prefs.end();
}

void eTomadaSalvaRele(Rele *rele) {
  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock) {
    logaMensagem("eTomadaSalvaRele: erro de mutex!");
    return;
  }

  eTomadaSalvaReleUnsafe(rele);
}

void eTomadaSalvaSensorUnsafe(Sensor *sensor) {
  Preferences prefs;
  prefs.begin("sensores", false);
  
  setPrefsAtr(prefs, sensor->num, "nome",  String(sensor->nome));
  setPrefsAtr(prefs, sensor->num, "pino",  String(sensor->pino));
  setPrefsAtr(prefs, sensor->num, "tipo",  String(sensor->tipo));

  prefs.end();
}

void eTomadaSalvaSensor(Sensor *sensor) {
  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock) {
    logaMensagem("eTomadaSalvaSensor: erro de mutex!");
    return;
  }

  eTomadaSalvaSensorUnsafe(sensor);
}

void eTomadaFactoryReset() {
  {
    MutexLock lockPrefs(prefsMutex, pdMS_TO_TICKS(2500));
    MutexLock lockReles(releMutex, pdMS_TO_TICKS(2500));
    MutexLock lockSensores(sensorMutex, pdMS_TO_TICKS(2500));
    if (!lockPrefs || !lockReles || !lockSensores) {
      logaMensagem("Erro de mutex no factory reset!");
      return;
    }

    Preferences prefs;
    prefs.begin("reles", false);
    prefs.clear();
    prefs.end();
    prefs.begin("sensores", false);
    prefs.clear();
    prefs.end();

    logaMensagem("Factory Reset: Gravando Reles:");
    int totReles = relesGetCount();
    for (int r=1; r <= totReles; r++) {
      Rele *rele = releGet(r);
      memcpy(rele, &relesConfigDefault[r - 1], sizeof(Rele));

      eTomadaSalvaReleUnsafe(rele);
      relePrint(rele);
    }
    
    logaMensagem("Factory Reset: Gravando Sensores:");
    int totSensores = sensoresGetCount();
    for (int s=1; s <= totSensores; s++) {
      Sensor *sensor = sensorGet(s);
      memcpy(sensor, &sensoresConfigDefault[s - 1], sizeof(Sensor));

      eTomadaSalvaSensorUnsafe(sensor);
      sensorPrint(sensor);
    }
  }

  processaRegras();
}

bool eTomadaPinoOutOK(int pino) {
  int totPinosOK = sizeof(pinosOutOK) / sizeof(pinosOutOK[0]);
  for (int i=0; i < totPinosOK; i++) {
    if (pino == pinosOutOK[i]) return true;
  }
  return false;
}

bool eTomadaPinoInOK(int pino) {
  int totPinosOK = sizeof(pinosInOK) / sizeof(pinosInOK[0]);
  for (int i=0; i < totPinosOK; i++) {
    if (pino == pinosInOK[i]) return true;
  }
  return false;
}
