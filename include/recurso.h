#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "rele.h"
#include "sensor.h"
#include "botao.h"
#include "umidificador.h"
#include "tipoRecurso.h"
#include "eventos.h"

struct Recurso
{
  char id[32];
  TipoRecurso tipo;
  char nome[32];
  union
  {
    Rele *rele;
    Sensor *sensor;
    Botao *botao;
    Umidificador *umid;
  };
};

void recursosInit();
int recursosGetCount();
void recursosZera();

Recurso *recursoGet(const char *id);
Recurso *recursoGetPorIndice(int posicao);

// Altera o recurso > acoes
bool recursoSetFromJSON(uint8_t *json, String &recursoIDOut, String &msgOut, bool enviaEventos = true);
bool recursoSet(const char *recursoID, String estado, String &msgOut, bool enviaEventos = true);

const char *recursoGetTipoStr(TipoRecurso tipo);
TipoRecurso recursoGetTipoFromStr(String tipoStr);
JsonDocument recursoGetJSONDoc(Recurso *r);

String recursoGetJSONEvento(const char *recursoID, TipoEvento tipoEvento);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoEnviaSSE(const char *recursoID);

int recursoGetValor(Recurso *r);
void recursoPrint(Recurso *recurso);
