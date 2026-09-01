#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "R2B2",
    .board = "esp32c3",
    .btnResetPin = 255,
    .ledPin = 4,
    .ledInvertido = false,
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
    .umidificador = {0, 0},
};
