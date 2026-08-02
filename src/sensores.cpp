#include <Arduino.h>

#include "eTomada.h"
#include "hardwareProfile.h"
#include "sensores.h"
#include "tipoSensores.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"
#include "prefs.h"
#include "recurso.h"

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Sensor sensores[MAX_SENSORES];

static int boardSensorCount = 0;

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
    sensorLoadFromPrefs(sensor, s, prefs);

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

void sensorPrint(Sensor *sensor)
{
  TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);

  logaMensagem("Sensor %d:%d:%s (%s) > [%s - %s]",
               sensor->num, sensor->pino, sensor->nome,
               (sensor->ativo ? "on" : "off"),
               tipoSensor ? tipoSensor->tipo : "",
               tipoSensor ? tipoSensor->nome : "");
}

void sensorLoadFromPrefs(Sensor *sensor, int num, Preferences &prefs)
{
  sensor->num = num;

  strncpy(sensor->nome, getPrefsAtr(prefs, num, "nome").c_str(), sizeof(sensor->nome) - 1);
  sensor->nome[sizeof(sensor->nome) - 1] = '\0';

  sensor->valor = 0;

  // Falta verificar se o TipoSensor inicializou OK
  sensor->ativo = false;
}

// REQUIRE sensorMutex locked
JsonDocument sensorGetJSONDoc(Sensor *s, bool full)
{
  JsonDocument doc;

  doc["num"] = s->num;
  doc["nome"] = s->nome;
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

String sensorAtualizaConfigFromJSON(Recurso *recurso, JsonDocument doc)
{
  if (recurso->tipo != RECURSO_SENSOR)
  {
    return "Recurso nao é SENSOR!";
  }

  MutexLock lock(sensorMutex, pdMS_TO_TICKS(2500));
  if (!lock)
  {
    return "mutex timeout";
  }

  Sensor *sensor = recursoGetSensor(recurso);

  if (!doc["nome"].isNull())
  {
    String nome = doc["nome"].as<String>();
    if (nome == "")
    {
      nome = "??";
    }
    strncpy(sensor->nome, nome.c_str(), sizeof(sensor->nome) - 1);
    sensor->nome[sizeof(sensor->nome) - 1] = '\0';
  }

  // Setar no prefs
  eTomadaSalvaSensor(recurso);

  sensorPrint(sensor);

  return "OK";
}

void sensoresAtualiza()
{
  int totS = sensoresGetCount();
  String jsonAtualiza[totS];

  {
    MutexLock lock(sensorMutex);
    if (!lock)
    {
      logaMensagem("sensorAtualiza: mutex timeout");
      return;
    }

    for (int s = 1; s <= totS; s++)
    {
      Sensor *sensor = &sensores[s - 1];

      if (!sensor->ativo || sensor->pino == -1)
      {
        // Desativado
        continue;
      }

      TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
      if (!tipoSensor)
      {
        logaMensagem("Sensor[%d] tipo invalido [%p]", s, sensor->tipo);
        continue;
      }
      if (tipoSensor->status != "OK")
      {
        logaMensagem("Sensor[%d] tipo inativo [%s]. Inativando sensor", s, tipoSensor->nome);
        sensor->ativo = false;
        continue;
      }

      int novoValor = tipoSensor->lerSensor(sensor);

      if (sensor->valor != novoValor)
      {
        // MUDOU valor do sensor
        sensor->valor = novoValor;

        jsonAtualiza[s - 1] = sensorGetJSONString(sensor);
      }
    }
  }

  // Enviar os eventos sem o mutex
  for (int s = 0; s < totS; s++)
  {
    if (jsonAtualiza[s] != "")
    {
      httpEnviaEvento(jsonAtualiza[s], "sse_sensor");
    }
  }
}
