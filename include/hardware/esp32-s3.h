#pragma once
#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "ESP32-S3",
    .ledPin = 255, // TODO :: controlar o led RGB!
    .reles = {
        {10, true},
        {11, true},
        {12, true},
        {13, true},
        {255, true}, // FIM
    },
    .sensores = {
        {"LUXXPTO", 1},
        {"UmidXPTO", 2},
        {"", 255}, // FIM
    },
    .botoes = {
        //{6},
        //{7},
        {255}, // FIM
    },
};
