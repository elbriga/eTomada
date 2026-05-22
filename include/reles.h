#pragma once

#include <Arduino.h>
#include <Preferences.h>

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
Rele *releLoadFromPrefs(int num, Preferences &prefs);

String releControlaUnsafe(int numRele, bool estado, int override = 0);
String releControla(int numRele, bool estado, int override = 0);

String relesSetFromJSON(uint8_t *json);
String releAtualizaConfigFromJSON(uint8_t *json);
