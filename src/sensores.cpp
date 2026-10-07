#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
#include "eventos.h"
#include "sensorChuva.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("SENSOR", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Sensor *sensores = NULL;
static int boardSensorCount = 0;

void sensoresInit()
{
  logaM(LOG_NORMAL, "Inicializando Sensores Locais");

  // Verificar quantos sensores temos
  boardSensorCount = 0;
  for (int s = 0; s < MAX_SENSORES; s++)
  {
    SensorHW sHW = hardwareProfile.sensores[s];
    if (sHW.pino == 255)
      break;
    boardSensorCount++;
  }

  sensores = new Sensor[boardSensorCount]();
  if (!sensores)
    utilDIE("Nao foi possivel alocar memoria para sensores");

  // Inicializar os TipoSensor
  tipoSensorInit();

  Preferences prefs;
  prefs.begin("sensores", false);

  // Para testes
  // prefs.putString("naoGravarS1", "NAO");
  // prefs.putString("deltaS3", "10");

  int totSensores = sensoresGetCount();
  for (int s = 1; s <= totSensores; s++)
  {
    Sensor *sensor = sensorGet(s);

    sensor->num = s;
    sensor->valor = 0;

    String key = "naoGravarS" + String(sensor->num);
    sensor->gravarEventos = prefs.getString(key.c_str()) != "NAO";

    // default
    sensor->delta = 1;
    sensor->maxSilencioSecs = 3600; // 1 hora
    sensor->ultimoValorServer = 0;

    key = "deltaS" + String(sensor->num);
    if (prefs.isKey(key.c_str()))
    {
      sensor->delta = prefs.getString(key.c_str()).toInt();
      logaM(LOG_AVISO, "Sensor[S%d] com delta ativado: [%d]", sensor->num, sensor->delta);
    }
    key = "maxSilencioS" + String(sensor->num);
    if (prefs.isKey(key.c_str()))
      sensor->maxSilencioSecs = prefs.getString(key.c_str()).toInt();

    SensorHW sHW = hardwareProfile.sensores[s - 1];
    if (strlen(sHW.sensorID))
    {
      strcpy(sensor->tipo, sHW.sensorID);
      sensor->pino = sHW.pino;

      TipoSensor *ts = tipoSensorGet(sensor->tipo);
      if (ts)
      {
        strlcpy(sensor->categoria, ts->tipo, sizeof(sensor->categoria));
        strlcpy(sensor->unidade, ts->unidade, sizeof(sensor->unidade));
        // TODO : Atualizar quando o status mudar!
        strlcpy(sensor->status, ts->status.c_str(), sizeof(sensor->status));
      }
    }
  }

  prefs.end();
}

int sensoresGetCount()
{
  return boardSensorCount;
}

Sensor *sensorGet(int numSensor)
{
  return (numSensor > 0 && numSensor <= sensoresGetCount())
             ? &sensores[numSensor - 1]
             : NULL;
}

JsonDocument sensorGetJSONDoc(Recurso *r, bool full)
{
  JsonDocument doc;

  Sensor *s = recursoGetSensor(r);
  if (!s)
    return doc;

  doc["num"] = s->num;
  doc["tipo"] = s->tipo;

  if (full)
  {
    doc["pino"] = s->pino;

    int valor = !strcmp(r->id, SENSORCHUVA_RECURSOID) ? sensorChuvaGetHorasSemChuva() : s->valor;
    doc["valor"] = valor;

    doc["categoria"] = s->categoria;
    doc["unidade"] = s->unidade;
    doc["status"] = s->status;
  }

  return doc;
}

void sensoresAtualizaTask(void *args);

void sensoresAtualiza()
{
  if (!sensoresGetCount() && !sensorChuvaAtivo())
    return;

  xTaskCreate(
      sensoresAtualizaTask,
      "sensoresAtz",
      4096,
      NULL,
      1,
      NULL);
}

void sensoresAtualizaTask(void *args)
{
  { // Escopo para o lock (sem ele não chama o destrutor)
    MutexLock lock(modeloMutex, "sensoresAtualizaTask");
    if (!lock)
    {
      logaM(LOG_CRITICO, "sensorAtualiza: mutex timeout");
      vTaskDelete(NULL);
      return;
    }

    sensorChuvaLoopLocked();

    int totRecursos = recursosGetCount();
    for (int r = 0; r < totRecursos; r++)
    {
      Recurso *rec = recursoGetPorIndice(r);
      if (rec->tipo != RECURSO_SENSOR)
        continue;
      if (rec->remoto)
        continue;

      Sensor *sensor = rec->sensor;

      if (sensor->pino == -1)
      {
        // Desativado
        continue;
      }
      TipoSensor *tipoSensor = tipoSensorGet(sensor->tipo);
      if (!tipoSensor)
      {
        logaM(LOG_CRITICO, "Sensor[%s] tipo invalido [%p]", rec->id, sensor->tipo);
        continue;
      }
      if (tipoSensor->status != "OK")
      {
        logaM(LOG_AVISO, "Sensor[%s] tipo inativo [%s]. Pulando sensor", rec->id, tipoSensor->nome);
        continue;
      }

      sensor->valor = tipoSensor->lerSensor(sensor);

      // Sensor de chuva tem os eventos postados pelo modulo sensorChuva.cpp
      if (strcmp(rec->id, SENSORCHUVA_RECURSOID))
      {
        uint32_t agora = millis();

        // Verificar delta e maxSilencio
        bool gravarEvento = sensor->gravarEventos &&
                            (abs(sensor->ultimoValorServer - sensor->valor) >= sensor->delta ||
                             (agora - sensor->ultimoEventoServer > sensor->maxSilencioSecs * 1000));
        if (gravarEvento)
        {
          sensor->ultimoValorServer = sensor->valor;
          sensor->ultimoEventoServer = agora;
        }

        eventoPost(EVENTO_VALOR_MUDOU, rec->id, true, true, gravarEvento);
      }
    }
  }

  vTaskDelete(NULL);
}
