#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "U1",
    .board = "esp32",
    .ledPin = 14,
    .reles = {
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // FIM
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {27, 13}, // Ativando Umidificador
};
