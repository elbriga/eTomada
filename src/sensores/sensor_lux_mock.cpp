#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

static String sensorLuxInit() {
  return "OK";
}

static int sensorLuxLer(Sensor *s) {
  // Ler sensor de LUX
  return s->valor + (esp_random() % 21) - 10; // MOCK
}

TipoSensor sensorLux = {
    "LUXXPTO",
    "LUX",
    "L",
    sensorLuxInit,
    sensorLuxLer
};
