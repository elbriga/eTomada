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
int pinosOK[] = { 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 };

// Salvar as regras na memoria FLASH
String getPrefsAtr(Preferences &prefs, int num, String nomeAtr);
String setPrefsAtr(Preferences &prefs, int num, String nomeAtr, String val);

bool eTomadaPinoOK(int pino) {
  int totPinosOK = sizeof(pinosOK) / sizeof(pinosOK[0]);
  for (int i=0; i < totPinosOK; i++) {
    if (pino == pinosOK[i]) return true;
  }
  return false;
}

void eTomadaLoadConfig() {
  Preferences prefs;

  logaMensagem("Carregando Configuracao dos reles:");

  prefs.begin("reles", false);

  // Para testes
  // prefs.putString("nome1", "Luz");
  // prefs.putString("regra1", "OF|02:00|07:59");
  // prefs.putString("pino1", "16");
  // prefs.putString("ativo1", "1");

  relesInit();

  Rele *rele;
  int totReles = relesGetCount();
  for (int r=1; r <= totReles; r++) {
    rele = releGet(r);
    if (!rele) {
      // TODO ERRO!
      continue;
    }

    rele->num = r;
    strncpy(rele->nome,  getPrefsAtr(prefs, r, "nome").c_str(),  sizeof(rele->nome) - 1);
    rele->nome[sizeof(rele->nome) - 1] = '\0';

    rele->pino = atoi(getPrefsAtr(prefs, r, "pino").c_str());

    strncpy(rele->regra, getPrefsAtr(prefs, r, "regra").c_str(), sizeof(rele->regra) - 1);
    rele->regra[sizeof(rele->regra) - 1] = '\0';

    String regraOK = validaRegra(rele->regra);
    if (regraOK != "OK") {
      logaMensagem("Regra [%s] INVALIDA! [%s] Desativando Rele[%d]", rele->regra, regraOK.c_str(), r);
    }
    bool pinoOK = eTomadaPinoOK(rele->pino);
    if (!pinoOK && rele->pino != -1) {
      logaMensagem("Pino [%d] INVALIDO! Desativando Rele[%d]", rele->pino, r);
    }
    rele->ativo = (regraOK == "OK" && pinoOK) ?
      (getPrefsAtr(prefs, r, "ativo") == "1") : false;

    // TODO :: guardar estado dos reles ativos e sem regra (modo manual) para voltar ao estado certo no boot
    rele->estado = 0;
    rele->override = 0;

    logaMensagem("Rele %d:%d:%s (%s) > [%s]",
      r, rele->pino, rele->nome, (rele->ativo ? "on" : "off"), rele->regra);
  }
  prefs.end();

  sensoresInit();

  Sensor *sensor;
// MOCK
sensor = sensorGet(1);
sensor->num  = 1;
sensor->tipo = tipoSensorGet(SENSORTIPO_temperatura);
strcpy(sensor->nome, "Temp");
sensor->valor = 0;
strcpy(sensor->valorStr, "");
sensor->pino = 1;

sensor = sensorGet(2);
sensor->num  = 2;
sensor->tipo = tipoSensorGet(SENSORTIPO_umidade);
strcpy(sensor->nome, "Umid");
sensor->valor = 0;
strcpy(sensor->valorStr, "");
sensor->pino = 2;

sensor = sensorGet(3);
sensor->num  = 3;
sensor->tipo = tipoSensorGet(SENSORTIPO_lux);
strcpy(sensor->nome, "lux");
sensor->valor = 0;
strcpy(sensor->valorStr, "");
sensor->pino = 3;

sensor = sensorGet(4);
sensor->num  = 4;
sensor->tipo = NULL;
strcpy(sensor->nome, "");
sensor->valor = 0;
strcpy(sensor->valorStr, "");
sensor->pino = -1;

  int totSensores = sensoresGetCount();
  for (int s=1; s <= totSensores; s++) {
      sensor = sensorGet(s);

      logaMensagem("Sensor %d:%d:%s (%s) > [%s]",
        s, sensor->pino, sensor->nome,
        (sensor->tipo ? "on" : "off"),
        sensor->tipo ? sensor->tipo->nome : "???"
      );
  }


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
      doc["erro"] = "mutex timeout";
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
    //MutexLock lock(releMutex, pdMS_TO_TICKS(2500));

    // if (!lock) {
    //   doc["erro"] = "mutex timeout";
    // } else {
      for (int i = 1; i <= totSensores; i++) {
        sensor = sensorGet(i);
        if (!sensor) continue;

        JsonObject s = sensores.add<JsonObject>();
        s["num"]      = sensor->num;
        s["tipo"]     = sensor->tipo->nome;
        s["nome"]     = sensor->nome;
        s["pino"]     = sensor->pino;
        s["valor"]    = sensor->valor;
        s["valorStr"] = sensor->valorStr;
      }
    // }
  }
  
  TipoSensor *ts;
  int totTS = tipoSensorGetCount();
  JsonArray tipoSensores = doc["tipoSensores"].to<JsonArray>();
  for (SensorType tipo = SENSORTIPO_temperatura;
     tipo < SENSORTIPO_MAX;
     tipo = (SensorType)(tipo + 1)) {
    ts = tipoSensorGet(tipo);
    if (!ts) continue;

    JsonObject t = tipoSensores.add<JsonObject>();
    t["num"]  = tipo;
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

String getPrefsAtr(Preferences &prefs, int num, String nomeAtr) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%s%d", nomeAtr.c_str(), num);
  return prefs.isKey(buff) ? prefs.getString(buff, "") : "";
}

String setPrefsAtr(Preferences &prefs, int num, String nomeAtr, String val) {
  String old = getPrefsAtr(prefs, num, nomeAtr);

  if (val != old) {
    char buff[32];
    snprintf(buff, sizeof(buff), "%s%d", nomeAtr.c_str(), num);
    prefs.putString(buff, val);
  }

  return old;
}
