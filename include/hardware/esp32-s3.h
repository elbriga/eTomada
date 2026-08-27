#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "R2B2", // :)
    .board = "esp32s3",
    .ledPin = 48, // LED RGB
    .ledInvertido = false,
    .reles = {
        {16, true},
        {17, true},
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // FIM
    },
    .botoes = {
        {5},
        {6},
        {255}, // FIM
    },
    .umidificador = {0, 0},
};
