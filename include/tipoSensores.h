#pragma once

struct Sensor;

enum SensorType {
    SENSORTIPO_DESATIVADO,
    SENSORTIPO_temperatura,
    SENSORTIPO_umidade,
    SENSORTIPO_lux,
    SENSORTIPO_MAX
};

struct TipoSensor {
    SensorType tipo;
    char nome[32];
    char format[32];
    void (*ler)(Sensor *s);
};

int tipoSensorGetCount();
TipoSensor *tipoSensorGet(SensorType tipo);
