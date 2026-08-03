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
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(int num);
NodoRemoto *nodoRemotoGetPorMAC(const char *mac);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
