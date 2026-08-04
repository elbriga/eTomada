#include "hardwareProfile.h"

const HardwareProfile hardwareProfile = {
    .modelo = "C3MINI",
    .reles = {
        {3, true},
        {2, true},
        {255, true}, // FIM
    },
    .sensores = {
        {"", 255}, // Indicar Vazio
    },
    .botoes = {
        {255}, // FIM
    },
};
