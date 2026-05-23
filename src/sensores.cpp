#include <Arduino.h>

#include "eTomada.h"
#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"
#include "prefs.h"

static Sensor sensores[MAX_SENSORES];

String sensorGetJSON(Sensor *s);

void sensoresInit() {
  // Zerar tudo
  memset(sensores, 0, sizeof(sensores));
  
  Preferences prefs;
  prefs.begin("sensores", false);

  // Para testes
  // prefs.putString("nome1", "Temp");
  // prefs.putString("tipo1", "Temperatura");
  // prefs.putString("pino1", "2");
  
  int totSensores = sensoresGetCount();
  for (int s=1; s <= totSensores; s++) {
      Sensor *sensor = sensorLoadFromPrefs(s, prefs);
      TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
      bool ativo = !!tipoSensor;

      logaMensagem("Sensor %d:%d:%s (%s) > [%s - %s]",
        s, sensor->pino, sensor->nome,
        (ativo ? "on" : "off"),
        ativo ? tipoSensor->tipo : "",
        ativo ? tipoSensor->nome : ""
      );
  }

  prefs.end();
}

int sensoresGetCount() {
  return MAX_SENSORES;
}

void sensoresAtualiza() {
  // logaMensagem("Atualizar Sensores");

  MutexLock lock(sensorMutex);
  if (!lock) {
    logaMensagem("sensorAtualiza: mutex timeout");
    return;
  }

  for (int s=1; s <= MAX_SENSORES; s++) {
    Sensor *sensor = &sensores[s-1];

    if (sensor->pino == -1) {
      // Desativado
      continue;
    }

    TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
    if (!tipoSensor) {
      logaMensagem("Sensor[%d] tipo invalido [%p]", s, sensor->tipo);
      continue;
    }

    int novoValor = tipoSensor->ler(sensor);
    String msg = sensorAtualizaUnsafe(s, novoValor);
    if (msg != "") {
      if (msg == "MUDOU") {
        String sensorJSON = sensorGetJSON(sensor);
        httpEnviaEvento(sensorJSON, "sse_sensor");
      } else {
        logaMensagem("Sensor[%d] => [%d] [%s]", s, novoValor, msg.c_str());
      }
    }
  }
}

// REQUIRE sensorMutex locked
String sensorAtualizaUnsafe(int numSensor, int valor)
{
  Sensor *sensor = sensorGet(numSensor);
  if (!sensor) {
    return "Sensor Invalido";
  }

  TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
  if (!tipoSensor) {
    return "Tipo Sensor Invalido";
  }

  String ret = "";
  if (sensor->valor != valor) {
    sensor->valor = valor;
    snprintf(sensor->valorStr, sizeof(sensor->valorStr),
      tipoSensor->format, valor);

    ret = "MUDOU";
  }

  return ret;
}

Sensor *sensorGet(int numSensor) {
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return NULL;
  }

  return &sensores[numSensor - 1];
}

// REQUIRE sensorMutex locked
String sensorGetJSON(Sensor *s) {
  JsonDocument doc;
  
  doc["num"] = s->num;
  doc["pino"] = s->pino;
  doc["nome"] = s->nome;
  doc["tipo"] = s->tipo;
  doc["valorStr"] = s->valorStr;
  doc["valor"] = s->valor;

  TipoSensor *ts = tipoSensorGet(s->tipo);
  doc["unidade"] = ts->tipo;

  String out;
  serializeJson(doc, out);

  return out;
}

String sensorAtualizaConfigFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }

  int numSensor = doc["sensor"];
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return "Sensor invalido";
  }

  Sensor sensorCopy;
  {
    MutexLock lock(sensorMutex, pdMS_TO_TICKS(2500));
    if (!lock) {
      return "mutex timeout";
    }

    Sensor *sensor = &sensores[numSensor - 1];

    int novoPino = doc["pino"].isNull() ? sensor->pino : atoi(doc["pino"].as<String>().c_str());
    if (novoPino != -1 && !eTomadaPinoInOK(novoPino)) {
      return "Pino Invalido";
    }

    if (!doc["tipo"].isNull()) {
      TipoSensor *ts = NULL;
      String novoTipo = doc["tipo"].as<String>();
      if (novoTipo != "") {
        ts = tipoSensorGet(novoTipo.c_str());
        if (!ts) {
          return "Tipo ["+novoTipo+"] Invalido";
        }
      }
      strncpy(sensor->tipo, novoTipo.c_str(), sizeof(sensor->tipo) - 1);
      sensor->tipo[sizeof(sensor->tipo) - 1] = '\0';
    }

    sensor->pino = novoPino;

    if (!doc["nome"].isNull()) {
      strncpy(sensor->nome, doc["nome"].as<String>().c_str(), sizeof(sensor->nome) - 1);
      sensor->nome[sizeof(sensor->nome) - 1] = '\0';
    }

    memcpy(&sensorCopy, sensor, sizeof(Sensor));
  }
  
  logaMensagem(">> SENSOR [%d] nome[%s] pino[%d] tipo[%d]",
    numSensor, sensorCopy.nome, sensorCopy.pino, sensorCopy.tipo);
  
  // Setar no prefs
  eTomadaSalvaSensor(&sensorCopy);

  String sensorJSON = sensorGetJSON(&sensorCopy);
  httpEnviaEvento(sensorJSON, "sse_sensor");

  return "OK";
}

Sensor *sensorLoadFromPrefs(int num, Preferences &prefs) {
  Sensor *sensor = sensorGet(num);
  if (!sensor) {
    logaMensagem("ERRO no sensor [%d]", num);
    return NULL;
  }

  sensor->num = num;
  strncpy(sensor->nome,  getPrefsAtr(prefs, num, "nome").c_str(),  sizeof(sensor->nome) - 1);
  sensor->nome[sizeof(sensor->nome) - 1] = '\0';

  sensor->pino = atoi(getPrefsAtr(prefs, num, "pino").c_str());  
  bool pinoOK = eTomadaPinoInOK(sensor->pino);
  if (!pinoOK) {
    sensor->tipo[0] = '\0';
    if (sensor->pino != -1) {
      logaMensagem("Pino [%d] INVALIDO! Desativando Sensor[%d]", sensor->pino, num);
    }
  } else {
    String tipo = getPrefsAtr(prefs, num, "tipo");
    TipoSensor *ts = tipoSensorGet(tipo.c_str());
    if (!ts && tipo != "") {
      logaMensagem("Tipo [%s] INVALIDO! Desativando Sensor[%d]", tipo.c_str(), num);
      tipo = "";
    }
    strncpy(sensor->tipo, tipo.c_str(), sizeof(sensor->tipo) - 1);
    sensor->tipo[sizeof(sensor->tipo) - 1] = '\0';
  }

  sensor->valor = 0;
  sensor->valorStr[0] = '\0';

  return sensor;
}
