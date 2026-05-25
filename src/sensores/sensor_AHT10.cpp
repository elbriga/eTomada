#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 aht;

static bool sensorAHT10Inicializado = false;

String sensorAHT10Init() {
  if (!sensorAHT10Inicializado) {
    if (!aht.begin()) {
      return "Falha init AHT10!";
    }
  }

  sensorAHT10Inicializado = true;
  return "OK";
}

int sensorAHT10LerTemperatura() {
  // Ler sensor de temperatura
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  return (int)(temp.temperature * 100);
}

int sensorAHT10LerUmidade() {
  // Ler sensor de temperatura
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  return (int)(temp.relative_humidity * 100);
}
