#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define NODOS_PATH "/nodosRemotos.json"

enum TipoNodoRemoto
{
  TIPO_NODO_DESCONHECIDO = 0,
  TIPO_NODO_LITE = 13,
  TIPO_NODO_FULL = 33
};

struct NodoRemoto
{
  char id[32];
  TipoNodoRemoto tipo;
  IPAddress ip;
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(const char *id);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
void nodosRemotosLimpaCacheNovosNodos();
