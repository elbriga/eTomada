#include <Arduino.h>

#include "eTomada.h"
#include "mestre.h"
#include "hardwareProfile.h"
#include "sensor.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"
#include "prefs.h"
#include "recurso.h"
#include "anunciador.h"

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Sensor sensores[MAX_SENSORES];

static int boardSensorCount = 0;

// struct temporaria usada em sensoresAtualiza
struct AtualizacaoSensor
{
  Recurso *rec;
  int novoValor;
  bool mudou;
  bool desativar;
};

void sensoresInit()
{
  // Zerar tudo
  memset(sensores, 0, sizeof(sensores));

  // Verificar quantos sensores temos
  boardSensorCount = 0;
  for (int s = 0; s < MAX_SENSORES; s++)
  {
    SensorHW sHW = hardwareProfile.sensores[s];
    if (sHW.pino == 255)
      break;
    boardSensorCount++;
  }

  // Inicializar os TipoSensor
  tipoSensorInit();

  Preferences prefs;
  prefs.begin("sensores", false);

  // Para testes
  // prefs.putString("nome1", "Temp de Fora");

  int totSensores = sensoresGetCount();
  for (int s = 1; s <= totSensores; s++)
  {
    Sensor *sensor = sensorGet(s);

    sensor->num = s;
    sensor->valor = 0;

    SensorHW sHW = hardwareProfile.sensores[s - 1];
    TipoSensor *tipoSensor = NULL;
    if (strlen(sHW.sensorID))
    {
      strcpy(sensor->tipo, sHW.sensorID);
      sensor->pino = sHW.pino;

      tipoSensor = tipoSensorGet(sensor->tipo);
    }

    sensor->ativo = !!tipoSensor;
    if (sensor->ativo)
    {
      if (tipoSensor->status != "OK")
      {
        logaMensagem("Erro ao inicializar sensor: %s", tipoSensor->status.c_str());
        sensor->ativo = false;
      }
      else
      {
        // Sensores que usam os ADCs
        if (!strcmp(sensor->tipo, "ACS712"))
        {
          pinMode(sensor->pino, INPUT);
        }
      }
    }
    else
    {
      if (strlen(sensor->tipo) > 0)
      {
        logaMensagem("TipoSensor [%s] INVALIDO! Desativando Sensor[%d]", sensor->tipo, s);
      }
    }

    sensorPrint(sensor);
  }

  prefs.end();
}

int sensoresGetCount()
{
  return boardSensorCount;
}

Sensor *sensorGet(int numSensor)
{
  if (numSensor < 1 || numSensor > sensoresGetCount())
  {
    return NULL;
  }

  return &sensores[numSensor - 1];
}

void sensorPrint(Sensor *sensor) // TODO :: substituir por recursoPrint
{
  TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);

  logaMensagem("Sensor %d:%d (%s) > [%s - %s]",
               sensor->num, sensor->pino,
               (sensor->ativo ? "on" : "off"),
               tipoSensor ? tipoSensor->tipo : "",
               tipoSensor ? tipoSensor->nome : "");
}

// REQUIRE sensorMutex locked
JsonDocument sensorGetJSONDoc(Sensor *s, bool full)
{
  JsonDocument doc;

  doc["num"] = s->num;
  doc["tipo"] = s->tipo;

  if (full)
  {
    doc["pino"] = s->pino;
    doc["valor"] = s->valor;
    doc["ativo"] = s->ativo;

    TipoSensor *ts = tipoSensorGet(s->tipo);
    doc["categoria"] = ts ? ts->tipo : "???";
    doc["unidade"] = ts ? ts->unidade : "?-?";
  }

  return doc;
}

// REQUIRE sensorMutex locked
String sensorGetJSONString(Sensor *s)
{
  String out;
  JsonDocument doc = sensorGetJSONDoc(s, true);

  serializeJson(doc, out);
  return out;
}

void sensoresAtualiza()
{
  int maxSensores = sensoresGetCount();
  if (!maxSensores)
    return;

  int totRecursos = recursosGetCount(RECURSO_TODOS);

  AtualizacaoSensor *atual = new AtualizacaoSensor[maxSensores]();

  // Ler os sensores sem o Lock
  int totSensoresOK = 0;
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (rec->tipo != RECURSO_SENSOR)
      continue;
    if (rec->remoto)
      continue;

    Sensor *sensor = rec->sensor;

    if (!sensor->ativo || sensor->pino == -1)
    {
      // Desativado
      continue;
    }
    TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
    if (!tipoSensor)
    {
      logaMensagem("Sensor[%s] tipo invalido [%p]", rec->id, sensor->tipo);
      continue;
    }

    int idx = totSensoresOK++;
    atual[idx].rec = rec;

    if (tipoSensor->status != "OK")
    {
      logaMensagem("Sensor[%s] tipo inativo [%s]. Inativando sensor", rec->id, tipoSensor->nome);
      atual[idx].desativar = true;
      continue;
    }

    atual[idx].novoValor = tipoSensor->lerSensor(sensor);
  }

  // Atualizar os recrusos SENSORES COM LOCK
  {
    MutexLock lock(recursosMutex);
    if (!lock)
    {
      delete[] atual;
      logaMensagem("sensorAtualiza: mutex timeout");
      return;
    }

    for (int rs = 0; rs < totSensoresOK; rs++)
    {
      Recurso *rec = atual[rs].rec;
      Sensor *sensor = rec->sensor;

      if (atual[rs].desativar)
      {
        sensor->ativo = false;
        continue;
      }

      atual[rs].mudou = (sensor->valor != atual[rs].novoValor);
      sensor->valor = atual[rs].novoValor;
    }
  }

  // Enviar os Eventos e os SSE sem Lock
  for (int rs = 0; rs < totSensoresOK; rs++)
  {
    if (!atual[rs].mudou)
      continue;

    // recursoEnviaSSE(atual[rb].rec) e mestreEnviaEvento(atual[rb].rec) em outra thread
    anunciadorPost({ANUNCIO_RECURSO, atual[rs].rec, true});
  }

  delete[] atual;
}
