#include "sensores.h"
#include "tipoSensores.h"

void sensorTemperaturaLer(SensorConfig *self, char *out, int outLen);
void sensorUmidadeLer(SensorConfig *self, char *out, int outLen);
void sensorLUXLer(SensorConfig *self, char *out, int outLen);

static TipoSensor sensoresDisponiveis[3] = {
  { SENSORTIPO_temperatura, "Temperatura", sensorTemperaturaLer },
  { SENSORTIPO_umidade, "Umidade", sensorUmidadeLer },
  { SENSORTIPO_lux, "LUX", sensorLUXLer },
};


void sensorTemperaturaLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "25 C");
}

void sensorUmidadeLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "80%");
}

void sensorLUXLer(SensorConfig *self, char *out, int outLen) {
  snprintf(out, outLen, "33");
}

int tipoSensorGetCount() {
  return sizeof(sensoresDisponiveis) / sizeof(sensoresDisponiveis[0]);
}

TipoSensor *tipoSensorGet(SensorType tipo) {
  int totTS = tipoSensorGetCount();
  for (int i=0; i < totTS; i++) {
    if (sensoresDisponiveis[i].tipo == tipo) {
      return &sensoresDisponiveis[i];
    }
  }
  return NULL;
}
