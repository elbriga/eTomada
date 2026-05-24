#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"

#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 aht;

static String sensorTemperaturaAHT10Init() {
  if (!aht.begin()) {
    return "Falha ao inicializar o sensor AHT10!";
  }
  return "OK";
}

static int sensorTemperaturaAHT10Ler(Sensor *s) {
  // Ler sensor de temperatura
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  return (int)(temp.temperature * 100);
}

TipoSensor sensorTemperaturaAHT10 = {
    "AHT10",
    "Temperatura",
    "%.2f° C",
    true,
    sensorTemperaturaAHT10Init,
    sensorTemperaturaAHT10Ler
};
