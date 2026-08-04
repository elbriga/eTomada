#pragma once

#include "rele.h"
#include "sensores.h"
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
    const char *modelo;
    ReleHW reles[MAX_RELES];
    SensorHW sensores[MAX_SENSORES];
    // BotaoHW botoes[MAX_BOTOES];
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
