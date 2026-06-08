#include "sensores.h"
#include "tipoSensores.h"

extern TipoSensor sensorTemperaturaAHT10;
extern TipoSensor sensorUmidadeAHT10;
extern TipoSensor sensorUmidade;
extern TipoSensor sensorLuxBH1750;
extern TipoSensor sensorLux;
extern TipoSensor sensorCorrenteACS712;

static TipoSensor sensoresDisponiveis[] = {
  sensorTemperaturaAHT10,
  sensorUmidadeAHT10,
  sensorUmidade,
  sensorLuxBH1750,
  sensorLux,
  sensorCorrenteACS712,
};

void tipoSensorInit() {
  for (int t=0; t < tipoSensorGetCount(); t++) {
    TipoSensor *ts = tipoSensorGetPorId(t);
    ts->num = t + 1;
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

JsonDocument tipoSensorGetJSONDoc(TipoSensor *ts) {
  JsonDocument doc;

  doc["num"]    = ts->num;
  doc["nome"]   = ts->nome;
  doc["tipo"]   = ts->tipo;
  doc["status"] = ts->status;

  return doc;
}
