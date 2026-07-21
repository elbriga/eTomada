#pragma once

#include "eTomada.h"

#ifdef HW_LOLIN
    #include "hardware/lolin.h"
#elif defined(HW_C3MINI)
    #include "hardware/c3mini.h"
#else
    #error "Nenhum Hardware Profile definido."
#endif

extern const HardwareProfile hardwareProfile;
