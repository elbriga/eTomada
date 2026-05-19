#include "sensores.h"
#include "loga.h"

static SensorConfig sensores[MAX_SENSORES];

int sensoresGetCount() {
  return MAX_SENSORES;
}

void sensoresAtualiza() {
  logaMensagem("Atualizar Sensores");
}

SensorConfig *sensorGet(int numSensor) {
  if (numSensor < 1 || numSensor > MAX_SENSORES) {
    return NULL;
  }

  return &sensores[numSensor - 1];
}
