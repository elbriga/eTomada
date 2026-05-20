#pragma once

#define MAX_SENSORES 4

struct Sensor {
    int tipo; // SensorType
    int num;
    int pino;
    char nome[32];
    char valorStr[32];
    int valor;
};

void sensoresInit();

int sensoresGetCount();
Sensor *sensorGet(int numSensor);

void sensoresAtualiza();
