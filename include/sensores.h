#pragma once

#include "tipoSensores.h"

#define MAX_SENSORES 4

struct Sensor {
    TipoSensor *tipo;
    int num;
    int pino;
    char nome[32];
    char valorStr[32];
    int valor;
};

void sensoresInit();

int sensoresGetCount();
Sensor *sensorGet(int numSensor);
void sensorSet(Sensor *sensor, int valor);

void sensoresAtualiza();
