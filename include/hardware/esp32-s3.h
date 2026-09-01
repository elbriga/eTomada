#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "R8S3B1", // :)
    .board = "esp32s3",
    .btnResetPin = 5,
    .ledPin = 48, // LED RGB
    .ledInvertido = false,
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
        {6},
        {255}, // FIM
    },
    .umidificador = {0, 0},
};
