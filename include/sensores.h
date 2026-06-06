#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

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

Sensor *sensorLoadFromPrefs(int num, Preferences &prefs);
JsonDocument sensorGetJSONDoc(Sensor *s);

void sensoresAtualiza();

String sensorAtualizaConfigFromJSON(uint8_t *json);
