#include <Arduino.h>

#define MAX_SENSORES 4

typedef struct SensorConfig {
    int num;
    bool ativo;
    int pino;
    char nome[32];
    char valor[32];
};

enum SensorType {
    temperatura,
    umidade,
    lux,
};

typedef struct Sensor {
    SensorType tipo;
    char nome[32];
    void (*ler)(SensorConfig *self, char *out, int outLen);
};

void sensoresAtualiza();
