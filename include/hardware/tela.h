#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "TELA",
    .board = "esp32dev",
    .ledPin = 255,
    .ledInvertido = false,
    .ledRGB = false,
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
};
