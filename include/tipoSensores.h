#pragma once

struct Sensor;

struct TipoSensor {
    const char *nome;
    const char *format;
    void (*ler)(Sensor *s);
};

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(const char *nome);
TipoSensor *tipoSensorGetAux(int i);
