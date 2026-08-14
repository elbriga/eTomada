#pragma once

#include "rele.h"
#include "sensor.h"
#include "botao.h"
typedef struct
{
    int pino;
    bool invertido;
} ReleHW;

typedef struct
{
    const char *sensorID;
    int pino;
} SensorHW;

typedef struct
{
    int pino;
    // TODO :: bool invertido;
} BotaoHW;

typedef struct
{
    const char *modelo;
    int ledPin;
    ReleHW reles[MAX_RELES];
    SensorHW sensores[MAX_SENSORES];
    BotaoHW botoes[MAX_BOTOES];
} HardwareProfile;

#ifdef HW_LOLIN
#include "hardware/lolin.h"
#elif defined(HW_C3MINI)
#include "hardware/c3mini.h"
#elif defined(HW_ESP32)
#include "hardware/esp32.h"
#else
#error "Nenhum Hardware Profile definido."
#endif
