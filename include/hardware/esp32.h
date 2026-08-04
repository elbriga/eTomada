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
    //.gpioBotoes = { 20,21, 255 },
};
