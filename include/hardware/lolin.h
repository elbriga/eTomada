#pragma once

#include "eTomada.h"

const HardwareProfile hardwareProfile = {
    .modelo = "LOLIN",
    .gpioReles = { 16,13,15,12,14,0,2,3 },
    .gpioBotoes = { 255 },
    .relesInvertidos = false,
};
