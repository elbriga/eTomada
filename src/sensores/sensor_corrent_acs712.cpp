#include <Arduino.h>

#include "sensor.h"
#include "tipoSensores.h"

static int sensorCorrenteACS712Pino = 255;
static int sensorCorrenteACS712Leitura = 0;
void sensorCorrenteACS712Task(void *args);

static String sensorCorrenteACS712Init(int pino)
{
  sensorCorrenteACS712Pino = pino;

  xTaskCreatePinnedToCore(
      sensorCorrenteACS712Task,
      "sensorCorrenteACS712",
      4096,
      NULL,
      1,
      NULL,
      1);

  return "OK";
}

void sensorCorrenteACS712Task(void *args)
{
  int tempoDeLeitura = 50; // ms

  while (1)
  {
    long inicio = millis();
    int min = INT_MAX, max = 0;
    while (millis() - inicio < tempoDeLeitura)
    {
      uint16_t val = analogRead(sensorCorrenteACS712Pino);
      if (val > max)
        max = val;
      if (val < min)
        min = val;

      vTaskDelay(pdTICKS_TO_MS(1));
    }
    sensorCorrenteACS712Leitura = max - min;

    // Serial.printf(">>>>> Lido ACS712: %d\n", sensorCorrenteACS712Leitura);

    vTaskDelay(pdTICKS_TO_MS(8000));
  }
}

static int sensorCorrenteACS712Ler(Sensor *s)
{
  return sensorCorrenteACS712Leitura;
}

TipoSensor sensorCorrenteACS712 = {
    "ACS712",
    "Corrente",
    "A",
    sensorCorrenteACS712Init,
    sensorCorrenteACS712Ler};
