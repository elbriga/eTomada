#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

typedef enum {
    RECURSO_RELE = 1,
    RECURSO_SENSOR = 2,
    RECURSO_BOTAO = 3,
} TipoRecurso;

struct Recurso {
  TipoRecurso tipo;
  // TODO trazer o "nome" para ca
  int num;
  bool remoto;
  void *device;         // Rele / Sensor / Botao / NodoRemoto
};

void recursosInit();
int recursosGetCount();
Recurso *recursoGet(TipoRecurso tipo, int num);
void recursoPrint(Recurso *recurso);
