#pragma once

#include <Preferences.h>

#include "tipoSensores.h"

#define MAX_SENSORES 4

struct Sensor {
    int num;
    int pino;
    char nome[32];
    char tipo[32];
    char valorStr[32];
    int valor;
};

void sensoresInit();
int sensoresGetCount();
Sensor *sensorGet(int numSensor);
Sensor *sensorLoadFromPrefs(int num, Preferences &prefs);
String sensorAtualizaConfigFromJSON(uint8_t *json);

void sensoresAtualiza();
String sensorAtualizaUnsafe(int numSensor, int valor);
