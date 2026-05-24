#pragma once

struct Sensor;

struct TipoSensor {
    const char *nome;
    const char *tipo;
    const char *format;
    bool ehFloat;
    String (*inicializaSensor)();
    int (*lerSensor)(Sensor *s);
    bool ativo;
};

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(const char *nome);
TipoSensor *tipoSensorGetPorId(int i);
