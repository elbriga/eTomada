#pragma once

#include <Arduino.h>

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
    int onPin;
    int umidPin;
    int fanPin;
} UmidificadorHW;

typedef struct
{
    const char *modelo; // Modelo do eTomada - config de reles, sensores e botoes
    const char *board;  // board do esp
    int ledPin;
    bool ledInvertido;
    bool ledRGB;
    ReleHW reles[MAX_RELES];
    SensorHW sensores[MAX_SENSORES];
    BotaoHW botoes[MAX_BOTOES];
    UmidificadorHW umidificador; // se onPin != 255 && umidPin != 255 => eTomada Umidificador!
} HardwareProfile;

#ifdef HW_LOLIN
#include "hardware/lolin.h"
#elif defined(HW_MESTRE)
#include "hardware/mestre.h"
#elif defined(HW_TERRARIO)
#include "hardware/terrario.h"
#elif defined(HW_UMIDFANVALVE)
#include "hardware/umidFanValve.h"
#elif defined(HW_UMIDTERRARIO)
#include "hardware/umidTerrario.h"
#elif defined(HW_TELA)
#include "hardware/tela.h"
#elif defined(HW_GROW)
#include "hardware/grow.h"
#else
#error "Nenhum Hardware Profile definido."
#endif
