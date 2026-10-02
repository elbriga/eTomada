#pragma once
#include "hardwareProfile.h"

// umid-terrario
const HardwareProfile hardwareProfile = {
    .modelo = "U1",
    .board = "esp32",
    .ledPin = 255, // Desligar o pisca!
    .ledInvertido = false,
    .ledRGB = false,
    .reles = {
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // FIM
    },
    .botoes = {
        {255}, // FIM
    },
    .umidificador = {
        .onPin = 27,
        .umidPin = 13,
        .fanPin = 255,
    }, // Ativando Umidificador sem FAN
};
