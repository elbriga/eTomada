#include <Arduino.h>

#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"

static Sensor sensores[MAX_SENSORES];

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
    String msg = sensorAtualiza(s, novoValor);
    if (msg != "") {
      logaMensagem("Sensor[%d] => [%d] [%s]", s, novoValor, msg.c_str());
    }
  }
}

String sensorAtualiza(int numSensor, int valor)
{
  // MutexLock lock(sensorMutex);
  // if (!lock) {
  //   return "sensorAtualiza: mutex timeout";
  // }

  return sensorAtualizaUnsafe(numSensor, valor);
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

    httpEnviaEvento("{ \"valor\": "+String(valor)+" }", "sensor");

    ret = "MUDOU!";
  }

  return ret;
}

Sensor *sensorGet(int numSensor) {
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return NULL;
  }

  return &sensores[numSensor - 1];
}
