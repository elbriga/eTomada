#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Recurso; // Forward declaration

#define MAX_SENSORES 4

struct Sensor
{
    int num;
    int pino;
    char tipo[32];
    int valor;
};

void sensoresInit();
int sensoresGetCount();
Sensor *sensorGet(int numSensor);

JsonDocument sensorGetJSONDoc(Recurso *r, bool full);

void sensoresAtualiza();
