#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "TELA",
    .board = "esp32dev",
    .led = {
        .pino = 255,
        .invertido = false,
        .rgb = false,
    },
    .reles = {
        {255, false},
    },
    .sensores = {
        {"", 255},
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {.onPin = 255},
    .mic = {.sckPin = 255},
};
