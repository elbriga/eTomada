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
#include "umidificador.h"

#define RECURSOS_REMOTOS_PATH "/recursosRemotos.json"

struct RecursoRemoto
{
  TipoRecurso tipo;
  char idLocal[32];  // ID Local do recurso dentro do tipo. ex.: R10
  char idRemoto[32]; // ID Remoto do recurso dentro do tipo. ex.: R2
  NodoRemoto *nodo;
  bool del;
  union
  {
    Rele rele;
    Sensor sensor;
    Botao botao;
    Umidificador umid;
  };
};

void recursosRemotosInit();
int recursosRemotosGetCount();
RecursoRemoto *recursoRemotoGet(const char *id);
RecursoRemoto *recursoRemotoGetPorIndice(int i);

String recursoRemotoAddFromJSON(uint8_t *json);
String recursoRemotoDelFromJSON(uint8_t *json);

void recursoRemotoAtualizaFromSnapshotLocked(NodoRemoto *nodo, JsonDocument &snapshot);
String recursosRemotosPersisteLocked(RecursoRemoto *novoRecurso = nullptr);

void recursoRemotoPrint(RecursoRemoto *r);
