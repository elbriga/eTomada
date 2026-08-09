#pragma once
#include "recurso.h"

enum TipoEvento
{
    EVENTO_NENHUM,

    EVENTO_LIGOU,
    EVENTO_DESLIGOU,
    EVENTO_TOGGLE,

    EVENTO_PRESSIONOU,
    EVENTO_SOLTOU,
    EVENTO_CLICK,
    EVENTO_DOUBLE_CLICK,
    EVENTO_LONG_PRESS,

    EVENTO_VALOR_MUDOU,

    EVENTO_HORARIO,
};

typedef struct
{
    TipoEvento tipo;
    Recurso *recurso;
    bool enviaSSE;
    bool enviaMestre;
} Evento;

void eventosInit();
void eventoPost(TipoEvento tipo,
                Recurso *recurso,
                bool enviaSSE,
                bool enviaMestre);
