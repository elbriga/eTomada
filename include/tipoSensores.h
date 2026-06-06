#pragma once
#include <ArduinoJson.h>

struct Sensor;

struct TipoSensor {
    const char *nome;
    const char *tipo;
    const char *unidade;
    String (*inicializaSensor)();
    int (*lerSensor)(Sensor *s);
    String status;
    int num;
};

void tipoSensorInit();

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(const char *nome);
TipoSensor *tipoSensorGetPorId(int i);
JsonDocument tipoSensorGetJSONDoc(TipoSensor *ts);
