#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 aht;

static bool sensorAHT10Inicializado = false;
static sensors_event_t humidity, temp;
static long ultimaLeitura = 0;

String sensorAHT10Init() {
  if (!sensorAHT10Inicializado) {
    if (!aht.begin()) {
      return "Falha init AHT10!";
    }
  }

  sensorAHT10Inicializado = true;
  return "OK";
}

void sensorAHT10Ler() {
  if (millis() - ultimaLeitura < 10) {
    return;
  }
  ultimaLeitura = millis();
  aht.getEvent(&humidity, &temp);
}

int sensorAHT10LerTemperatura() {
  sensorAHT10Ler();
  return (int)(temp.temperature);
}

int sensorAHT10LerUmidade() {
  sensorAHT10Ler();
  return (int)(humidity.relative_humidity);
}
