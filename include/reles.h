#pragma once
#include <Arduino.h>

#define MAX_RELES 8

struct Rele {
  int num;
  int pino;
  bool estado;
  String nome;
  String regra;
  bool ativo;
};

int relesGetCount();
String relesAtualizaConfigFromJSON(uint8_t *json);

Rele *releGet(int numRele);
String releControla(int numRele, bool estado);
