#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

void sensorTemperaturaLer(Sensor *s);
void sensorUmidadeLer(Sensor *s);
void sensorLUXLer(Sensor *s);

static TipoSensor sensoresDisponiveis[] = {
  { SENSORTIPO_temperatura, "Temperatura", "%d° C", sensorTemperaturaLer },
  { SENSORTIPO_umidade,     "Umidade",     "%d %%", sensorUmidadeLer },
  { SENSORTIPO_lux,         "LUX",         "%d L",  sensorLUXLer },
};

static int temp=12, umid=50, LUX=200; // MOCK

void sensorTemperaturaLer(Sensor *s) {
  // Ler sensor de temperatura
  temp += (esp_random() % 6) - 3; // MOCK

  sensorSet(s, temp);
}

void sensorUmidadeLer(Sensor *s) {
  umid += (esp_random() % 10) - 5; // MOCK

  sensorSet(s, umid);
}

void sensorLUXLer(Sensor *s) {
  LUX += (esp_random() % 14) - 7; // MOCK

  sensorSet(s, LUX);
}

int tipoSensorGetCount() {
  return sizeof(sensoresDisponiveis) / sizeof(sensoresDisponiveis[0]);
}

TipoSensor *tipoSensorGet(SensorType tipo) {
  int totTS = tipoSensorGetCount();
  for (int i=0; i < totTS; i++) {
    if (sensoresDisponiveis[i].tipo == tipo) {
      return &sensoresDisponiveis[i];
    }
  }
  return NULL;
}
