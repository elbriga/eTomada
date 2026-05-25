#include "loga.h"
#include "sensores.h"
#include "tipoSensores.h"
#include "tipoSensor_AHT10.h"

static String sensorTemperaturaAHT10Init() {
  return sensorAHT10Init();
}

static int sensorTemperaturaAHT10Ler(Sensor *s) {
  return sensorAHT10LerTemperatura();
}

TipoSensor sensorTemperaturaAHT10 = {
    "AHT10t",
    "Temperatura",
    "%.2f° C",
    true,
    sensorTemperaturaAHT10Init,
    sensorTemperaturaAHT10Ler
};
