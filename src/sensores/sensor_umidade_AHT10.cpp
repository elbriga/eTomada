#include "sensores.h"
#include "tipoSensores.h"
#include "tipoSensor_AHT10.h"

static String sensorUmidadeAHT10Init() {
  return sensorAHT10Init();
}

static int sensorUmidadeAHT10Ler(Sensor *s) {
  return sensorAHT10LerUmidade();
}

TipoSensor sensorUmidadeAHT10 = {
    "AHT10u",
    "Umidade",
    "%%",
    sensorUmidadeAHT10Init,
    sensorUmidadeAHT10Ler
};
