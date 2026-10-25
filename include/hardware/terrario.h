#pragma once
#include "hardwareProfile.h"

// Embaixo da escada! C3MINI
const HardwareProfile hardwareProfile = {
    .modelo = "R2B2",
    .board = "esp32c3",
    .led = {
        .pino = 4,
        .invertido = false,
        .rgb = false,
    },
    .reles = {
        {3, true},
        {2, true},
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // Indicar Vazio
    },
    .botoes = {
        {0},
        {1},
        {255}, // FIM
    },
    .umidificador = {.onPin = 255},
    .mic = {.sckPin = 255},
};
