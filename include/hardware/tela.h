#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "R5S4B2",
    .board = "esp32dev",
    .ledPin = 2,
    .ledInvertido = false,
    .reles = {
        {27, false},
        {26, false},
        {25, false},
        {33, false},
        {32, false},
        {255, false},
    },
    .sensores = {
        {"AHT10t", 0},
        {"AHT10u", 0},
        {"ACS712", 36},
        {"", 255}, // FIM
    },
    .botoes = {
        {17},
        {255}, // FIM
    },
    .umidificador = {0, 0}, // Ativando Umidificador
};
