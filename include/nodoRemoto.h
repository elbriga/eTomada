#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define NODOS_PATH "/nodosRemotos.json"

struct NodoRemoto
{
  char id[32];
  IPAddress ip;
  bool online;
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(const char *id);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
void nodosRemotosLimpaCacheNovosNodos();
