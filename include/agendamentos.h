#pragma once
#include <Arduino.h>

struct AcaoAgendada
{
    char recursoID[8];
    bool estado;
    bool ativa;
    uint32_t quando;
};

void agendamentosInit();
void agendamentosAdd(const char *recursoID, bool estado, int timeoutMs);
