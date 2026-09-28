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
  bool estado;
  bool invertido;
};

void relesInit();
int relesGetCount();
Rele *releGet(int numRele);

JsonDocument releGetJSONDoc(Recurso *r, bool full);

bool releControlaLocked(Rele *rele, bool estado, String &msgOut);
bool releControla(Rele *rele, bool estado, String &msgOut);
