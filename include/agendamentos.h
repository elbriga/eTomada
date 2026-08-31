#pragma once
#include <Arduino.h>

enum TipoAgendamento
{
    AGEND_NENHUM,
    AGEND_RECURSO,
    AGEND_RESET,
};

void agendamentosInit();
void agendamentosAdd(TipoAgendamento tipo, int timeoutMs, const char *recursoID = "", int estado = 0);
