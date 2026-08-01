#pragma once

#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "LOLIN-OLED",
    .reles = {
        {16, false},
        {13, false},
        {15, false},
        {12, false},
        {14, false},
        {0, false},
        {2, false},
        {3, false},
    },
    .sensores = {
        {"AHT10t", 0},
        {"AHT10u", 0},
        {"BH1750", 0},
        {"ACS712", 39},
    },
    //.gpioBotoes = { 255 },
};
