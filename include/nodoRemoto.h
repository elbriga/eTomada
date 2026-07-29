#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define MAX_NODOS_REMOTOS 8

struct NodoRemoto {
  int num;
  char deviceID[32];
  IPAddress ip;
};

void nodoRemotoInit();
int nodosRemotosGetCount();
NodoRemoto *nodoRemotoGet(int num);
void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodosRemotosRefresh();
