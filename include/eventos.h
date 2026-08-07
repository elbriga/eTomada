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

    EVENTO_VALOR_MUDOU
};

typedef struct
{
    TipoEvento tipo;
    Recurso *recurso;
} Evento;

void eventosInit();
void eventoPost(Evento e);
