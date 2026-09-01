#pragma once
#include <Arduino.h>

struct Recurso; // Forward declaration

enum TipoEvento
{
    EVENTO_NENHUM = 0,

    EVENTO_LIGOU = 10,
    EVENTO_DESLIGOU = 20,
    EVENTO_TOGGLE = 30,

    // EVENTO_PRESSIONOU,
    // EVENTO_SOLTOU,
    EVENTO_CLICK = 100,
    EVENTO_DOUBLE_CLICK = 110,
    EVENTO_LONG_PRESS = 120,

    EVENTO_VALOR_MUDOU = 200,

    EVENTO_HORARIO = 300,
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

String eventoMockFromJson(uint8_t *json);
