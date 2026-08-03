#include "sensores.h"
#include "tipoSensores.h"
#include "hardwareProfile.h"
#include "loga.h"

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

extern TipoSensor sensorTemperaturaAHT10;
extern TipoSensor sensorUmidadeAHT10;
extern TipoSensor sensorUmidade;
extern TipoSensor sensorLuxBH1750;
extern TipoSensor sensorLux;
extern TipoSensor sensorCorrenteACS712;

static TipoSensor sensoresDisponiveis[] = {
    sensorTemperaturaAHT10,
    sensorUmidadeAHT10,
    sensorUmidade,
    sensorLuxBH1750,
    sensorLux,
    sensorCorrenteACS712,
};

void tipoSensorInit()
{
  for (int t = 0; t < tipoSensorGetCount(); t++)
  {
    TipoSensor *ts = tipoSensorGetPorIndice(t);
    ts->num = t + 1;

    // Verificar se temos esse sensor no hardware
    bool temos = false;
    for (int s = 0; s < MAX_SENSORES; s++)
    {
      if (!strcmp(hardwareProfile.sensores[s].sensorID, ts->nome))
      {
        temos = true;
        break;
      }
      if (hardwareProfile.sensores[s].pino == 255)
      { // EOF!
        break;
      }
    }

    if (temos)
    {
      logaMensagem("Inicializando sensor [%s]", ts->nome);
      ts->status = ts->inicializaSensor();
    }
    else
    {
      ts->status = "OFF";
    }
  }
}

int tipoSensorGetCount()
{
  return sizeof(sensoresDisponiveis) / sizeof(sensoresDisponiveis[0]);
}

TipoSensor *tipoSensorGet(const char *nome)
{
  int totTS = tipoSensorGetCount();
  for (int i = 0; i < totTS; i++)
  {
    if (!strcmp(sensoresDisponiveis[i].nome, nome))
    {
      return &sensoresDisponiveis[i];
    }
  }
  return NULL;
}

TipoSensor *tipoSensorGetPorIndice(int i)
{
  if (i < 0 || i >= tipoSensorGetCount())
  {
    return NULL;
  }

  return &sensoresDisponiveis[i];
}

JsonDocument tipoSensorGetJSONDoc(TipoSensor *ts)
{
  JsonDocument doc;

  doc["num"] = ts->num;
  doc["nome"] = ts->nome;
  doc["tipo"] = ts->tipo;
  doc["status"] = ts->status;

  return doc;
}
