#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

static int sensorTemperaturaLer(Sensor *s) {
  // Ler sensor de temperatura
  return s->valor + (esp_random() % 7) - 3; // MOCK
}

TipoSensor sensorTemperatura = {
    "Temperatura",
    "%d° C",
    sensorTemperaturaLer
};
