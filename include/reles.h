#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "recurso.h"

#define MAX_RELES 8

struct Rele {
  int num;
  int pino;
  char nome[32];
  char regra[32];
  bool ativo;
  bool estado;
  unsigned long override; // TS para sobrepor o estado manual as regras
};

void relesInit();
int relesGetCount();
Rele *releGet(int numRele);
void relePrint(Rele *rele);

void releLoadFromPrefs(Rele *r, int num, Preferences &prefs);
JsonDocument releGetJSONDoc(Rele *r, bool full);

String releControlaUnsafe(Rele *rele, bool estado, int override = 0);
String releControla(int numRele, bool estado, int override = 0);

String releAtualizaConfigFromJSON(Recurso *r, JsonDocument doc);
