#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"

#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 aht;

static String sensorUmidadeAHT10Init() {
  if (!aht.begin()) {
    return "Falha init AHT10!";
  }
  return "OK";
}

static int sensorUmidadeAHT10Ler(Sensor *s) {
  // Ler sensor de temperatura
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  return (int)(temp.temperature * 100);
}

TipoSensor sensorUmidadeAHT10 = {
    "AHT10",
    "Umidade",
    "%d %%",
    false,
    sensorUmidadeAHT10Init,
    sensorUmidadeAHT10Ler
};
