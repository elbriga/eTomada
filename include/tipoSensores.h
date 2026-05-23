#pragma once

struct Sensor;

struct TipoSensor {
    const char *nome;
    const char *tipo;
    const char *format;
    int (*ler)(Sensor *s);
};

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(const char *nome);
TipoSensor *tipoSensorGetPorId(int i);
