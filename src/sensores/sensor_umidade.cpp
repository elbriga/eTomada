#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

static int sensorUmidadeLer(Sensor *s) {
  // Ler sensor de umidade
  return s->valor + (esp_random() % 11) - 5; // MOCK
}

TipoSensor sensorUmidade = {
    "UmidXPTO",
    "Umidade",
    "%d %%",
    sensorUmidadeLer
};
