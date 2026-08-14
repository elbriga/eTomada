#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "C3MINI",
    .ledPin = 4,
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
};
