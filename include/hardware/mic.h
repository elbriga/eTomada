#pragma once
#include "hardwareProfile.h"

// C3-mini Novo MIC
const HardwareProfile hardwareProfile = {
    .modelo = "M1",
    .board = "esp32c3",
    .ledPin = 8,
    .ledInvertido = false,
    .ledRGB = true,
    .reles = {
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // Indicar Vazio
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {.onPin = 255}, // Sem
    .mic = {
        .sckPin = 2,
        .wsPin = 6,
        .sdPin = 5,
    },
};
