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

// Whitelist de pinos
int pinosOutOK[] = { 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 };
int pinosInOK[] = { 1, 2, 3, 4, 5 };

void eTomadaLoadConfig() {
  Preferences prefs;

  logaMensagem("Carregando Configuracao dos reles:");
  prefs.begin("reles", false);

  // Para testes
  // prefs.putString("nome1", "Luz");
  // prefs.putString("regra1", "OF|02:00|07:59");
  // prefs.putString("pino1", "16");
  // prefs.putString("ativo1", "1");

  relesInit(); // Zerar tudo

  Rele *rele;
  int totReles = relesGetCount();
  for (int r=1; r <= totReles; r++) {
    rele = releLoadFromPrefs(r, prefs);
    if (!rele) {
      continue;
    }

    logaMensagem("Rele %d:%d:%s (%s) > [%s]",
      r, rele->pino, rele->nome,
      (rele->ativo ? "on" : "off"), rele->regra);
  }
  prefs.end();


  logaMensagem("Carregando Configuracao dos Sensores:");
  prefs.begin("sensores", false);

  // Para testes
  // prefs.putString("nome1", "Temp");
  // prefs.putString("tipo1", "Temperatura");
  // prefs.putString("pino1", "2");
  
  sensoresInit(); // Zerar tudo

  Sensor *sensor;
  int totSensores = sensoresGetCount();
  for (int s=1; s <= totSensores; s++) {
      sensor = sensorLoadFromPrefs(s, prefs);
      if (!sensor) {
        continue;
      }

      logaMensagem("Sensor %d:%d:%s (%s) > [%s]",
        s, sensor->pino, sensor->nome,
        (sensor->tipo ? "on" : "off"),
        sensor->tipo ? sensor->tipo->nome : ""
      );
  }
  prefs.end();

  Serial.println("");
}

String eTomadaGetDataJSON() {
  JsonDocument doc;
  doc["api"]    = 1; // versão da API
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
        if (!rele) continue;

        JsonObject r = reles.add<JsonObject>();
        r["num"]      = rele->num;
        r["nome"]     = rele->nome;
        r["regra"]    = rele->regra;
        r["pino"]     = rele->pino;
        r["estado"]   = rele->estado;
        r["ativo"]    = rele->ativo;
        r["override"] = rele->override;
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
        if (!sensor) continue;

        JsonObject s = sensores.add<JsonObject>();
        s["num"]      = sensor->num;
        s["tipo"]     = sensor->tipo ? sensor->tipo->nome : "";
        s["nome"]     = sensor->nome;
        s["pino"]     = sensor->pino;
        s["valor"]    = sensor->valor;
        s["valorStr"] = sensor->valorStr;
      }
    }
  }
  
  TipoSensor *ts;
  int totTS = tipoSensorGetCount();
  JsonArray tipoSensores = doc["tipoSensores"].to<JsonArray>();
  for (int i=0; i < totTS; i++) {
    ts = tipoSensorGetPorId(i);
    if (!ts) continue;

    JsonObject t = tipoSensores.add<JsonObject>();
    t["num"]  = i;
    t["nome"] = ts->nome;
  }

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaSalvaRele(Rele *rele) {
  Preferences prefs;

  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock) {
    // TODO msg
    return;
  }

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

void eTomadaSalvaSensor(Sensor *sensor) {
  Preferences prefs;

  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock) {
    // TODO msg
    return;
  }

  prefs.begin("sensores", false);
  
  setPrefsAtr(prefs, sensor->num, "nome",  String(sensor->nome));
  setPrefsAtr(prefs, sensor->num, "pino",  String(sensor->pino));
  setPrefsAtr(prefs, sensor->num, "tipo",  String(sensor->tipo ? sensor->tipo->nome : ""));

  prefs.end();
}

void eTomadaFactoryReset() {
  int totReles = relesGetCount();
  
  {
    MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
    if (!lock) {
      // TODO msg
      return;
    }

    Preferences prefs;
    prefs.begin("reles", false);
    prefs.clear();
    // TODO :: prefs.putString("totReles", String(totReles));
    prefs.end();
  }

  Rele *rele;
  for (int r=1; r <= totReles; r++) {
    rele = releGet(r);
    memcpy(rele, &relesConfigDefault[r - 1], sizeof(Rele));
    eTomadaSalvaRele(rele);
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
