#pragma once
#include "hardwareProfile.h"

// C3-mini DEV02
const HardwareProfile hardwareProfile = {
    .modelo = "R2L1",
    .board = "esp32c3",
    .led = {
        .pino = 8,
        .invertido = false,
        .rgb = true,
    },
    .reles = {
        {.pino = 5, .invertido = false},
        {.pino = 6, .invertido = false},
        {.pino = 255}, // FIM
    },
    .sensores = {
        {"", 255}, // Indicar Vazio
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {.onPin = 255}, // Sem
    .mic = {.sckPin = 255},
};
