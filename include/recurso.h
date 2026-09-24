#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "rele.h"
#include "sensor.h"
#include "botao.h"
#include "recursoRemoto.h"
#include "tipoRecurso.h"
#include "eventos.h"

struct Recurso
{
  char id[32];
  TipoRecurso tipo;
  char nome[32];
  bool remoto;
  union
  {
    Rele *rele;
    Sensor *sensor;
    Botao *botao;
    Umidificador *umid;
    RecursoRemoto *recursoRemoto;
  };
};

void recursosInit();
int recursosGetCount();
void recursosZera();

Recurso *recursoGet(const char *id);
Recurso *recursoGetPorIndice(int posicao);

Rele *recursoGetRele(Recurso *r);
Sensor *recursoGetSensor(Recurso *r);
Botao *recursoGetBotao(Recurso *recurso);
Umidificador *recursoGetUmidificador(Recurso *recurso);

// Altera o recurso > acoes
String recursoSetFromJSON(uint8_t *json, Recurso *&recursoOut, bool enviaMestre = true);
String recursoSet(Recurso *recurso, String estado, bool enviaMestre = true);
String recursoCheckLocked(Recurso *recurso, bool estadoDesejado);

// Atualiza o recurso > eventos
String recursoAtualizaFromJson(Recurso *recurso, JsonDocument doc, bool enviaEventos);

bool recursoSetNextID(RecursoRemoto *rr);
const char *recursoGetTipoStr(TipoRecurso tipo);
TipoRecurso recursoGetTipoFromStr(String tipoStr);
JsonDocument recursoGetJSONDoc(Recurso *r);

JsonDocument recursoGetJSONEvento(Recurso *r, TipoEvento tipoEvento);
String recursoEventoRecebido(uint8_t *json);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoEnviaSSE(Recurso *recurso);

int recursoGetValor(Recurso *r);
void recursoPrint(Recurso *recurso);
