#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "R8S3B2", // :)
    .board = "esp32s3",
    .ledPin = 48, // LED RGB
    .ledInvertido = false,
    .reles = {
        {21, false},
        {47, false},
        {48, false},
        {45, false},
        {35, false},
        {36, false},
        {37, false},
        {38, false},
    },
    .sensores = {
        {"AHT10t", 0},
        {"AHT10u", 0},
        //{"ACS712", 1},
        {"", 255}, // FIM
    },
    .botoes = {
        {5},
        {6},
        {255}, // FIM
    },
    .umidificador = {0, 0},
};
