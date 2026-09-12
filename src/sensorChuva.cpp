#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "recurso.h"
#include "sensor.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("CHUVA", nivel, fmt, ##__VA_ARGS__)

#define SENSORCHUVA_SECO 0
#define SENSORCHUVA_MOLHADO 1

#define SENSORCHUVA_DEBOUCE_TIME_CHUVA_MS 60000 // 1 minuto
#define SENSORCHUVA_DEBOUCE_TIME_SECO_MS 600000 // 10 minutos

struct SensorChuva
{
  Recurso *recurso;
  // debounce
  uint32_t debounce;
  bool ultimoEstado;
  // VARs
  bool estado;
  uint32_t tsInicioSeco;
};

static SensorChuva sensorChuva = {};

bool sensorChuvaAtivo()
{
  return sensorChuva.recurso != nullptr;
}

int sensorChuvaGetHorasSemChuva()
{
  if (!sensorChuva.tsInicioSeco)
    return 0; // Esta chovendo!

  return (millis() - sensorChuva.tsInicioSeco) / 1000 / 60 / 60;
}

void sensorChuvaInit()
{
  memset(&sensorChuva, 0, sizeof(SensorChuva));

  // Verificar se temos um sensor chamado CHUVA!
  Recurso *rChuva = recursoGet("CHUVA");
  if (!rChuva)
    return;
  if (rChuva->tipo != RECURSO_SENSOR)
    return;

  // Init
  sensorChuva.recurso = rChuva;
  sensorChuva.estado = SENSORCHUVA_SECO;
  sensorChuva.ultimoEstado = SENSORCHUVA_SECO;
  sensorChuva.debounce = millis();
  sensorChuva.tsInicioSeco = millis() - (3 * 24 * 60 * 60 * 1000); // Nao chove a 72h!

  logaM(LOG_NORMAL, "Inicializando sensor de CHUVA em [%s]", sensorChuva.recurso->id);
}

// Chamado de 10s/10s
void sensorChuvaLoop()
{
  if (!sensorChuvaAtivo())
    return;

  // Debounce
  Sensor *s = recursoGetSensor(sensorChuva.recurso);
  bool leitura = !s->valor ? SENSORCHUVA_MOLHADO : SENSORCHUVA_SECO; // PINO LOW == CHUVA ON
  if (leitura != sensorChuva.ultimoEstado)
  {
    sensorChuva.debounce = millis();
    sensorChuva.ultimoEstado = leitura;
  }

  bool mudou = false;
  if (sensorChuva.estado == SENSORCHUVA_MOLHADO)
  {
    // Verificar se parou de chover
    if (millis() - sensorChuva.debounce > SENSORCHUVA_DEBOUCE_TIME_SECO_MS && leitura == SENSORCHUVA_SECO)
    {
      sensorChuva.estado = SENSORCHUVA_SECO;
      sensorChuva.tsInicioSeco = millis();
      logaM(LOG_NORMAL, "SENSOR DE CHUVA MUDOU [SECO]");
      mudou = true;
    }
  }
  else
  {
    // Verificar se comecou a chover
    if (millis() - sensorChuva.debounce > SENSORCHUVA_DEBOUCE_TIME_CHUVA_MS && leitura == SENSORCHUVA_MOLHADO)
    {
      sensorChuva.estado = SENSORCHUVA_MOLHADO;
      sensorChuva.tsInicioSeco = 0;
      logaM(LOG_NORMAL, "SENSOR DE CHUVA MUDOU [CHUVA!]");
      mudou = true;
    }
  }

  if (mudou)
    eventoPost(EVENTO_VALOR_MUDOU, sensorChuva.recurso, true, true);
}