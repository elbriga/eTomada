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
  void *device;  // Rele / Sensor / Botao / NodoRemoto
};

void recursosInit();
int recursosGetCount(TipoRecurso tipo = RECURSO_TODOS);
Recurso *recursoGet(TipoRecurso tipo, int num);
Recurso *recursoGetPorId(int posicao);

JsonDocument recursoGetJSONDoc(Recurso *r, bool full);
void recursoPrint(Recurso *recurso);
