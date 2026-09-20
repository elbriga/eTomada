#pragma once
#include "hardwareProfile.h"

// C3-mini Novo Mestre sem recursos locais
const HardwareProfile hardwareProfile = {
    .modelo = "CONTROLE",
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
};
