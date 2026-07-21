#include <Arduino.h>

#include "eTomada.h"
#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"
#include "prefs.h"

static Sensor sensores[MAX_SENSORES];

void sensoresInit() {
  // Zerar tudo
  memset(sensores, 0, sizeof(sensores));

  // Inicializar os TipoSensor
  tipoSensorInit();
  
  Preferences prefs;
  prefs.begin("sensores", false);

  // Para testes
  // prefs.putString("nome1", "Temp de Fora");
  // prefs.putString("tipo1", "TempXPTO");
  // prefs.putString("pino1", "2");
  
  int totSensores = sensoresGetCount();
  for (int s=1; s <= totSensores; s++) {
    Sensor *sensor = sensorLoadFromPrefs(s, prefs);
    TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);

    sensor->ativo = !!tipoSensor;
    if (sensor->ativo) {
      if (tipoSensor->status != "OK") {
        logaMensagem("Erro ao inicializar sensor: %s", tipoSensor->status.c_str());
        sensor->ativo = false;
      } else {
        // Sensores que usam os ADCs
        if (!strcmp(sensor->tipo, "ACS712")) {
          pinMode(sensor->pino, INPUT);
        }
      }
    } else {
      if (strlen(sensor->tipo) > 0) {
        logaMensagem("TipoSensor [%s] INVALIDO! Desativando Sensor[%d]", sensor->tipo, s);
      }
    }

    sensorPrint(sensor);
  }

  prefs.end();
}

int sensoresGetCount() {
  return MAX_SENSORES;
}

Sensor *sensorGet(int numSensor) {
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return NULL;
  }

  return &sensores[numSensor - 1];
}

void sensorPrint(Sensor *sensor) {
  TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);

  logaMensagem("Sensor %d:%d:%s (%s) > [%s - %s]",
      sensor->num, sensor->pino, sensor->nome,
      (sensor->ativo ? "on" : "off"),
      tipoSensor ? tipoSensor->tipo : "",
      tipoSensor ? tipoSensor->nome : ""
    );
}

Sensor *sensorLoadFromPrefs(int num, Preferences &prefs) {
  Sensor *sensor = sensorGet(num);
  if (!sensor) {
    logaMensagem("ERRO no sensor [%d]", num);
    return NULL;
  }

  sensor->num = num;

  strncpy(sensor->nome, getPrefsAtr(prefs, num, "nome").c_str(),  sizeof(sensor->nome) - 1);
  sensor->nome[sizeof(sensor->nome) - 1] = '\0';

  sensor->pino = atoi(getPrefsAtr(prefs, num, "pino").c_str());  
  
  strncpy(sensor->tipo, getPrefsAtr(prefs, num, "tipo").c_str(), sizeof(sensor->tipo) - 1);
  sensor->tipo[sizeof(sensor->tipo) - 1] = '\0';

  sensor->valor = 0;

  // Falta verificar se o TipoSensor inicializou OK
  sensor->ativo = false;

  return sensor;
}

// REQUIRE sensorMutex locked
JsonDocument sensorGetJSONDoc(Sensor *s) {
  JsonDocument doc;
  TipoSensor *ts = tipoSensorGet(s->tipo);
  
  doc["num"] = s->num;
  doc["pino"] = s->pino;
  doc["nome"] = s->nome;
  doc["tipo"] = s->tipo;
  doc["categoria"] = ts ? ts->tipo : "???";
  doc["unidade"] = ts ? ts->unidade : "?-?";
  doc["valor"] = s->valor;
  doc["ativo"] = s->ativo;

  return doc;
}

// REQUIRE sensorMutex locked
String sensorGetJSONString(Sensor *s) {
  String out;
  JsonDocument doc = sensorGetJSONDoc(s);

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

    if (!doc["tipo"].isNull()) {
      String novoTipo = doc["tipo"].as<String>();
      if (novoTipo != "") {
        TipoSensor *ts = tipoSensorGet(novoTipo.c_str());
        if (!ts) {
          return "TipoSensor ["+novoTipo+"] Invalido";
        }
        // if (ts->status != "OK") {
        //   return "TipoSensor ["+novoTipo+"] Inativo ["+ts->status+"]";
        // }
      }
      strncpy(sensor->tipo, novoTipo.c_str(), sizeof(sensor->tipo) - 1);
      sensor->tipo[sizeof(sensor->tipo) - 1] = '\0';
    }

    sensor->ativo = !!strlen(sensor->tipo);

    if (!doc["nome"].isNull()) {
      String nome = doc["nome"].as<String>();
      if (nome == "") {
        nome = "??";
      }
      strncpy(sensor->nome, nome.c_str(), sizeof(sensor->nome) - 1);
      sensor->nome[sizeof(sensor->nome) - 1] = '\0';
    }

    memcpy(&sensorCopy, sensor, sizeof(Sensor));
  }
  
  logaMensagem(">> SENSOR [%d] nome[%s] pino[%d] tipo[%d]",
    numSensor, sensorCopy.nome, sensorCopy.pino, sensorCopy.tipo);
  
  // Setar no prefs
  eTomadaSalvaSensor(&sensorCopy);

  String sensorJSON = sensorGetJSONString(&sensorCopy);
  httpEnviaEvento(sensorJSON, "sse_sensor");

  return "OK";
}

void sensoresAtualiza() {
  // logaMensagem("Atualizar Sensores");
  String jsonAtualiza[MAX_SENSORES];

  {
    MutexLock lock(sensorMutex);
    if (!lock) {
      logaMensagem("sensorAtualiza: mutex timeout");
      return;
    }

    for (int s=1; s <= MAX_SENSORES; s++) {
      Sensor *sensor = &sensores[s-1];

      if (!sensor->ativo || sensor->pino == -1) {
        // Desativado
        continue;
      }

      TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
      if (!tipoSensor) {
        logaMensagem("Sensor[%d] tipo invalido [%p]", s, sensor->tipo);
        continue;
      }
      if (tipoSensor->status != "OK") {
        logaMensagem("Sensor[%d] tipo inativo [%s]. Inativando sensor", s, tipoSensor->nome);
        sensor->ativo = false;
        continue;
      }

      int novoValor = tipoSensor->lerSensor(sensor);

      if (sensor->valor != novoValor) {
        // MUDOU valor do sensor
        sensor->valor = novoValor;

        jsonAtualiza[s-1] = sensorGetJSONString(sensor);
      }
    }
  }

  // Enviar os eventos sem o mutex
  for (int s=0; s < MAX_SENSORES; s++) {
    if (jsonAtualiza[s] != "") {
      httpEnviaEvento(jsonAtualiza[s], "sse_sensor");
    }
  }
}
