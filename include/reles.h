#pragma once

#include <Arduino.h>

#define MAX_RELES 8

struct Rele {
  int num;
  int pino;
  char nome[32];
  char regra[32];
  bool ativo;
  bool estado;
};

int relesGetCount();
Rele *releGet(int numRele);
String releControla(int numRele, bool estado);

String relesSetFromJSON(uint8_t *json);
String relesAtualizaConfigFromJSON(uint8_t *json);
