#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "rele.h"
#include "sensor.h"
#include "botao.h"
#include "recursoRemoto.h"
#include "tipoRecurso.h"

struct Recurso
{
  char id[9];
  TipoRecurso tipo;
  char nome[32];
  bool remoto;
  unsigned long tsAtualizacao;
  union
  {
    Rele *rele;
    Sensor *sensor;
    Botao *botao;
    RecursoRemoto *recursoRemoto;
  };
};

void recursosInit();
int recursosGetCount(TipoRecurso tipo = RECURSO_TODOS);
Recurso *recursoGet(const char *id);
Recurso *recursoGetPorIndice(int posicao);

Rele *recursoGetRele(Recurso *r);
Sensor *recursoGetSensor(Recurso *r);
Botao *recursoGetBotao(Recurso *recurso);

// Altera o recurso > acoes
String recursoSetFromJSON(uint8_t *json, Recurso *&recursoOut, bool enviaMestre = true);
String recursoSet(Recurso *recurso, String estadoStr = "ON", bool enviaMestre = true);

// Atualiza o recurso > eventos - timestamp para ignorar eventos antigos
String recursoAtualizaFromJson(Recurso *recurso, JsonDocument doc, unsigned long timestamp);

const char *recursoGetTipoStr(TipoRecurso tipo);
JsonDocument recursoGetJSONDoc(Recurso *r);

JsonDocument recursoGetJSONEvento(Recurso *r);
String recursoEventoRecebido(uint8_t *json);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoEnviaSSE(Recurso *recurso);

void recursoPrint(Recurso *recurso);
