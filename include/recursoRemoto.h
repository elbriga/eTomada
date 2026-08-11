#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "eTomada.h"
#include "nodoRemoto.h"
#include "tipoRecurso.h"
#include "rele.h"
#include "sensor.h"
#include "botao.h"

struct RecursoRemoto
{
  TipoRecurso tipo;
  char idLocal[8];  // ID Local do recurso dentro do tipo. ex.: R10
  char idRemoto[8]; // ID Remoto do recurso dentro do tipo. ex.: R2
  NodoRemoto *nodo;
  union
  {
    Rele rele;
    Sensor sensor;
    Botao botao;
  };
};

void recursosRemotosInit();
int recursosRemotosGetCount();
RecursoRemoto *recursoRemotoGet(const char *id);
RecursoRemoto *recursoRemotoGetPorIndice(int i);

void recursoRemotoAtualizaFromSnapshot(NodoRemoto *nodo, JsonDocument *snapshot);

void recursoRemotoPrint(RecursoRemoto *r);
