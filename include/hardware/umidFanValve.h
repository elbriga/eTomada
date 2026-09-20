#pragma once
#include "hardwareProfile.h"

// C3-mini do Umidificador + Fan + valvula
const HardwareProfile hardwareProfile = {
    .modelo = "UV1",
    .board = "esp32c3",
    .ledPin = 255,
    .ledInvertido = false,
    .ledRGB = false,
    .reles = {
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // Indicar Vazio
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {
        .onPin = 2,
        .umidPin = 4,
        .fanPin = 3,
    }, // Ativando umid com fan
};
