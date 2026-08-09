#pragma once
#include <Arduino.h>

#include "eventos.h"

enum TipoCondicao
{
    COND_EVENTO,
    COND_HORARIO,
    COND_ESTADO,   // TODO
    COND_EXPRESSAO // TODO
};

enum TipoAcao
{
    ACAO_RECURSO,
    ACAO_DELAY, // TODO
    ACAO_SCRIPT // TODO
};

enum AcaoRecurso
{
    ACAO_ON,
    ACAO_OFF,
    ACAO_TOGGLE
};

enum Operador // TODO
{
    OP_EQ,
    OP_NE,
    OP_GT,
    OP_LT,
    OP_GE,
    OP_LE
};

struct Condicao
{
    TipoCondicao tipo;

    char recursoID[8]; // R1, S2, B1...

    union
    {
        TipoEvento evento;

        struct
        {
            Operador op;
            float valor;
        } estado;

        struct
        {
            uint8_t hora;
            uint8_t minuto;
        } horario;
    };
};

struct Acao
{
    TipoAcao tipo;

    char recursoID[8];

    union
    {
        AcaoRecurso comando;

        uint32_t delay;
    };
};

struct Regra
{
    uint16_t id;
    bool ativa;

    Condicao condicao;
    Acao acao;
};

void regrasInit();
void regrasProcessaEvento(Evento e);

String regraValida(String regra);

Regra regraCriaEvento(
    uint16_t id,
    const char *recursoID,
    TipoEvento evento,
    const char *acaoRecurso,
    AcaoRecurso comando);

Regra regraCriaHorario(
    uint16_t id,
    uint8_t hora,
    uint8_t minuto,
    const char *recursoID,
    AcaoRecurso comando);
