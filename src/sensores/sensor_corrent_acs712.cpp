#include <Arduino.h>

#include "sensores.h"
#include "tipoSensores.h"

static String sensorCorrenteACS712Init() {
  return "OK";
}

static int sensorCorrenteACS712Ler(Sensor *s) {
  int tempoDeLeitura = 500; // ms
  long inicio = millis();
  int min = INT_MAX, max = 0;
  while (millis() - inicio < tempoDeLeitura) {
    uint16_t val = analogRead(s->pino);
    if (val > max) max = val;
    if (val < min) min = val;
  }
  //logaMensagem("read(%d): %d a %d", s->pino, min, max);
  return max - min;
}

TipoSensor sensorCorrenteACS712 = {
    "ACS712",
    "Corrente",
    "A",
    sensorCorrenteACS712Init,
    sensorCorrenteACS712Ler
};
