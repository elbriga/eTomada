#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define MAX_NODOS_REMOTOS 8

struct NodoRemoto
{
  int num;
  char deviceID[32];
  IPAddress ip;
  JsonDocument snapshot;
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(int num);
JsonObject nodoGetRecursoSnapshot(NodoRemoto *nodo, String id);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
