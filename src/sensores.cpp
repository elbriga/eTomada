#include <Arduino.h>
#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"

static Sensor sensores[MAX_SENSORES];

void sensoresInit() {
  for (int i = 0; i < MAX_SENSORES; i++) {
    sensores[i].num  = i + 1;
    sensores[i].tipo = SENSORTIPO_DESATIVADO;
    sensores[i].pino = -1;
  }
}

int sensoresGetCount() {
  return MAX_SENSORES;
}

void sensoresAtualiza() {
  // logaMensagem("Atualizar Sensores");

  Sensor *sensor;
  TipoSensor *TS;
  for (int s=1; s <= MAX_SENSORES; s++) {
    sensor = &sensores[s-1];

    if (sensor->tipo == SENSORTIPO_DESATIVADO) {
      continue;
    }

    TS = tipoSensorGet((SensorType)sensor->tipo);
    if (!TS || !TS->ler) {
      logaMensagem("Sensor[%d] tipo invalido [%d]", s, sensor->tipo);
      continue;
    }

    TS->ler(sensor);
  }
}

Sensor *sensorGet(int numSensor) {
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return NULL;
  }

  return &sensores[numSensor - 1];
}
