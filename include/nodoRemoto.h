#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define MAX_NODOS_REMOTOS 8

struct NodoRemoto
{
  char id[8];
  char nome[32];
  char deviceID[32];
  IPAddress ip;
  bool online;
  int ping;
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(const char *id);
NodoRemoto *nodoRemotoGetPorMAC(const char *mac);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
