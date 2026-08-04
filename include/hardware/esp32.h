#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "ESP32",
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
