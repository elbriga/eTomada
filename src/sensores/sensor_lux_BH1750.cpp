#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>

#include "sensores.h"
#include "tipoSensores.h"

BH1750 lightMeter;

static String sensorLuxBH1750Init()
{
  Wire.begin();

  if (!lightMeter.begin())
  {
    return "Falha init BH1750";
  }

  return "OK";
}

static int sensorLuxBH1750Ler(Sensor *s)
{
  return lightMeter.readLightLevel() * 100;
}

TipoSensor sensorLuxBH1750 = {
    "BH1750",
    "LUX",
    "L",
    sensorLuxBH1750Init,
    sensorLuxBH1750Ler};
