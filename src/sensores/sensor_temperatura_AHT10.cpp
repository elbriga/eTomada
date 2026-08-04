#include "loga.h"
#include "sensor.h"
#include "tipoSensores.h"
#include "tipoSensor_AHT10.h"

static String sensorTemperaturaAHT10Init()
{
  return sensorAHT10Init();
}

static int sensorTemperaturaAHT10Ler(Sensor *s)
{
  return sensorAHT10LerTemperatura();
}

TipoSensor sensorTemperaturaAHT10 = {
    .nome = "AHT10t",
    .tipo = "Temperatura",
    .unidade = "° C",
    .inicializaSensor = sensorTemperaturaAHT10Init,
    .lerSensor = sensorTemperaturaAHT10Ler,
};
