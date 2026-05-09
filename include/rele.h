#pragma once
#include <Arduino.h>

struct Rele {
  int pino;
  bool estado;
  String nome;
  String regra;
};

extern Rele reles[8];

String releControla(int numRele, bool estado);
String releAtualizaConfigFromJSON(uint8_t *json);
