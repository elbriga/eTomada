#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "reles.h"
#include "sensores.h"
#include "recursoRemoto.h"
#include "tipoRecurso.h"

struct Recurso
{
  char id[9];
  TipoRecurso tipo;
  // TODO trazer o "nome" para ca
  int num;
  bool remoto;
  union
  {
    Rele *rele;
    Sensor *sensor;
    // Botao *botao;
    RecursoRemoto *recursoRemoto;
  };
};

void recursosInit();
int recursosGetCount(TipoRecurso tipo = RECURSO_TODOS);
Recurso *recursoGet(const char *id);
Recurso *recursoGetPorId(int posicao);

Rele *recursoGetRele(Recurso *r);
Sensor *recursoGetSensor(Recurso *r);

String recursoSetFromJSON(uint8_t *json, JsonDocument *docOut = nullptr);
String recursoSet(Recurso *recurso, bool estado, JsonDocument *jsonOut = nullptr);

const char *recursoGetTipoStr(TipoRecurso tipo);
JsonDocument recursoGetJSONDoc(Recurso *r);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoPrint(Recurso *recurso);
