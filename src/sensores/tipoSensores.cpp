#include "sensores.h"
#include "tipoSensores.h"

extern TipoSensor sensorTemperaturaAHT10;
extern TipoSensor sensorUmidadeAHT10;
extern TipoSensor sensorUmidade;
extern TipoSensor sensorLux;

static TipoSensor sensoresDisponiveis[] = {
  sensorTemperaturaAHT10,
  sensorUmidadeAHT10,
  sensorUmidade,
  sensorLux,
};

void tipoSensorInit() {
  for (int t=0; t < tipoSensorGetCount(); t++) {
    TipoSensor *ts = tipoSensorGetPorId(t);
    ts->status = ts->inicializaSensor();
  }
}

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

TipoSensor *tipoSensorGetPorId(int i) {
  if (i < 0 || i >= tipoSensorGetCount()) {
    return NULL;
  }

  return &sensoresDisponiveis[i];
}
