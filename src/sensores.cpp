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


// ==========================
void sensorTemperaturaLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "25 C");
}
void sensorUmidadeLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "80%");
}
void sensorLUXLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "33");
}

Sensor sensoresDisponiveis[3] = {
  { temperatura, "Temperatura", sensorTemperaturaLer },
  { umidade, "Umidade", sensorUmidadeLer },
  { lux, "LUX", sensorLUXLer },
};
