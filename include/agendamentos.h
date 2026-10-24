#pragma once
#include <Arduino.h>

#define AGENDAMENTO_TEMPO_MAXIMO (10 * 24 * 60 * 60)

enum TipoAgendamento
{
    AGEND_NENHUM,
    AGEND_RECURSO,
    AGEND_RESET,
};

void agendamentosInit();
void agendamentosAdd(TipoAgendamento tipo, int timeoutMs, const char *recursoID = "", const char *comando = "", uint16_t regraID = 0);
void agendamentosLimpa(const char *recursoID);
void agendamentosLimpaRegras();
