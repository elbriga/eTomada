#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

typedef enum {
  RECURSO_TODOS  = 0,
  RECURSO_RELE   = 1,
  RECURSO_SENSOR = 2,
  RECURSO_BOTAO  = 3,
} TipoRecurso;

struct Recurso {
  char id[9];
  TipoRecurso tipo;
  // TODO trazer o "nome" para ca
  int num;
  bool remoto;
  void *device;  // Rele / Sensor / Botao / RecursoRemoto
};

void recursosInit();
int recursosGetCount(TipoRecurso tipo = RECURSO_TODOS);
Recurso *recursoGet(const char *id);
Recurso *recursoGetPorId(int posicao);

String recursoSetFromJSON(uint8_t *json, JsonDocument &docOut);

const char *recursoGetTipoStr(TipoRecurso tipo);
JsonDocument recursoGetJSONDoc(Recurso *r, bool full);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoPrint(Recurso *recurso);
