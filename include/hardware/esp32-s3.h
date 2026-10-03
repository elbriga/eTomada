#pragma once
#include "hardwareProfile.h"

// grow
const HardwareProfile hardwareProfile = {
    .modelo = "R8S3B2",
    .board = "esp32s3",
    .ledPin = 255, // é 48 o RGB
    .ledInvertido = false,
    .ledRGB = true,
    .reles = {
        {21, false},
        {47, false},
        {45, false},
        {39, false},
        {40, false},
        {41, false},
        {42, false},
        {2, false},
        // {255, false},
    },
    .sensores = {
        {"AHT10t", 0},
        {"AHT10u", 0},
        {"ACS712", 1},
        {"", 255}, // FIM
    },
    .botoes = {
        {5},
        {6},
        {255}, // FIM
    },
    .umidificador = {.onPin = 255},
};
