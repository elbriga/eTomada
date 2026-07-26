#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "recurso.h"

#define MAX_SENSORES 4

struct Sensor {
    int num;
    int pino;
    char nome[32];
    char tipo[32];
    int valor;
    bool ativo;
};

void sensoresInit();
int sensoresGetCount();
Sensor *sensorGet(int numSensor);
void sensorPrint(Sensor *sensor);

void sensorLoadFromPrefs(Sensor *s, int num, Preferences &prefs);
JsonDocument sensorGetJSONDoc(Sensor *s, bool full);

void sensoresAtualiza();

String sensorAtualizaConfigFromJSON(Recurso *recurso, JsonDocument doc);
