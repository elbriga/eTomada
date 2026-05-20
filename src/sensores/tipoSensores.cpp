#include <Arduino.h>

#include "sensores.h"
#include "tipoSensores.h"

extern TipoSensor sensorTemperatura;
extern TipoSensor sensorUmidade;
extern TipoSensor sensorLux;

static TipoSensor sensoresDisponiveis[] = {
  sensorTemperatura,
  sensorUmidade,
  sensorLux,
};

int tipoSensorGetCount() {
  return sizeof(sensoresDisponiveis) / sizeof(sensoresDisponiveis[0]);
}

TipoSensor *tipoSensorGet(const char *nome) {
  int totTS = tipoSensorGetCount();
  for (int i=0; i < totTS; i++) {
    if (!strcmp(sensoresDisponiveis[i].nome, nome)) {
      return &sensoresDisponiveis[i];
    }
  }
  return NULL;
}

TipoSensor *tipoSensorGetAux(int i) {
  if (i < 0 || i >= tipoSensorGetCount()) {
    return NULL;
  }

  return &sensoresDisponiveis[i];
}
