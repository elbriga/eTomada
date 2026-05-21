#include <Arduino.h>

#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"

static Sensor sensores[MAX_SENSORES];

String sensorGetJSON(Sensor *s);

void sensoresInit() {
  for (int i = 0; i < MAX_SENSORES; i++) {
    sensores[i].num  = i + 1;
    sensores[i].tipo = NULL;
    sensores[i].pino = -1;
  }
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

  Sensor *sensor;
  for (int s=1; s <= MAX_SENSORES; s++) {
    sensor = &sensores[s-1];

    if (!sensor->tipo || sensor->pino == -1) {
      // Desativado
      continue;
    }

    if (!sensor->tipo->ler) {
      logaMensagem("Sensor[%d] tipo invalido [%p]", s, sensor->tipo);
      continue;
    }

    int novoValor = sensor->tipo->ler(sensor);
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

  String ret = "";
  if (sensor->valor != valor) {
    sensor->valor = valor;
    snprintf(sensor->valorStr, sizeof(sensor->valorStr), sensor->tipo->format, valor);

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
  doc["tipo"] = s->tipo ? s->tipo->nome : "???";
  doc["num"] = s->num;
  doc["pino"] = s->pino;
  doc["nome"] = s->nome;
  doc["valorStr"] = s->valorStr;
  doc["valor"] = s->valor;

  String out;
  serializeJson(doc, out);

  return out;
}
