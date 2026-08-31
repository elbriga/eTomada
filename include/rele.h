#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Recurso; // Forward declaration

#define MAX_RELES 8

struct Rele
{
  int num;
  int pino;
  bool ativo;
  bool estado;
  bool invertido;
};

void relesInit();
int relesGetCount();
Rele *releGet(int numRele);
void relePrint(Rele *rele);

JsonDocument releGetJSONDoc(Rele *r, bool full);

String releControlaLocked(Rele *r, bool estado);
String releControla(Rele *r, bool estado);
