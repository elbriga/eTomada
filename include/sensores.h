#pragma once

#include <Arduino.h>

#define MAX_SENSORES 4

struct Sensor {
    int num;
    bool ativo;
    int pino;
    char nome[32];
    char valor[32];
};

void sensoresAtualiza();
