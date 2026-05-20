#include <Arduino.h>

#include "esp_system.h" // MOCK

#include "sensores.h"
#include "tipoSensores.h"

void sensorTemperaturaLer(Sensor *s);
void sensorUmidadeLer(Sensor *s);
void sensorLUXLer(Sensor *s);

static TipoSensor sensoresDisponiveis[] = {
  { SENSORTIPO_temperatura, "Temperatura", sensorTemperaturaLer },
  { SENSORTIPO_umidade, "Umidade", sensorUmidadeLer },
  { SENSORTIPO_lux, "LUX", sensorLUXLer },
};

static int temp=12, umid=50, LUX=200; // MOCK

void sensorTemperaturaLer(Sensor *s) {
  temp += (esp_random() % 6) - 3; // MOCK

  s->valor = temp;
  snprintf(s->valorStr, sizeof(s->valorStr), "%d° C", s->valor);
}

void sensorUmidadeLer(Sensor *s) {
  umid += (esp_random() % 10) - 5; // MOCK

  s->valor = umid;
  snprintf(s->valorStr, sizeof(s->valorStr), "%d %%", s->valor);
}

void sensorLUXLer(Sensor *s) {
  LUX += (esp_random() % 14) - 7; // MOCK

  s->valor = LUX;
  snprintf(s->valorStr, sizeof(s->valorStr), "%d L", s->valor);
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
