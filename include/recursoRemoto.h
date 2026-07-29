#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "eTomada.h"
#include "nodoRemoto.h"
#include "tipoRecurso.h"

struct RecursoRemoto {
  TipoRecurso tipo;
  char id[8]; // ID local do recurso dentro do tipo. ex.: R10
  int numRR;  // Numero LOCAL dentro de prefs[recursoRemoto]
  int num;    // Numero REMOTO para acionamento
  NodoRemoto *nodo;
  union {
    Rele rele;
    Sensor sensor;
    //Botao botao;
  };
};

void recursosRemotosInit();
int recursosRemotosGetCount();
RecursoRemoto *recursoRemotoGet(const char *id);
RecursoRemoto *recursoRemotoGetPorId(int i) ;

JsonDocument recursoRemotoGetJSONDoc(RecursoRemoto *r, bool full);
void recursoRemotoPrint(RecursoRemoto *r);
