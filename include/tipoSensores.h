#pragma once

#include "sensores.h"

enum SensorType {
    SENSORTIPO_temperatura,
    SENSORTIPO_umidade,
    SENSORTIPO_lux,
    SENSORTIPO_MAX
};

struct TipoSensor {
    SensorType tipo;
    char nome[32];
    void (*ler)(SensorConfig *self, char *out, int outLen);
};

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(SensorType tipo);
