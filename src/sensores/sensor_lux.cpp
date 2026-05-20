#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

static int sensorLuxLer(Sensor *s) {
  // Ler sensor de LUX
  return s->valor;// + (esp_random() % 20) - 10; // MOCK
}

TipoSensor sensorLux = {
    "LUX",
    "%d L",
    sensorLuxLer
};
