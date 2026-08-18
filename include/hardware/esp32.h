#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "ESP32",
    .board = "esp32",
    .ledPin = 255, // SEM LED
    .reles = {
        {255, true}, // FIM
    },
    .sensores = {
        {"LUXXPTO", 1},
        {"UmidXPTO", 2},
        {"", 255}, // FIM
    },
    .botoes = {
        {3},
        {255}, // FIM
    },
};
