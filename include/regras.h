#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "eventos.h"

#define REGRAS_PATH "/automacoes.json"
#define REGRAS_PATH_DEFAULT "/config/automacoesDefault.json"

enum TipoCondicao
{
    COND_EVENTO = 10,
    COND_HORARIO = 20,
    COND_ESTADO = 30,   // TODO
    COND_EXPRESSAO = 40 // TODO
};

enum TipoAcao
{
    ACAO_ESTADO = 10,
    ACAO_TIMER = 20,
    ACAO_DELAY = 30, // TODO
    ACAO_SCRIPT = 40 // TODO
};

enum AcaoRecurso
{
    ACAO_ON = 10,
    ACAO_OFF = 20,
    ACAO_TOGGLE = 30,
    ACAO_PULSE = 40
};

enum Operador // TODO
{
    OP_EQ = 1,
    OP_NE = 2,
    OP_GT = 3,
    OP_LT = 4,
    OP_GE = 5,
    OP_LE = 6
};

struct Condicao
{
    TipoCondicao tipo;

    char recursoID[8]; // R1, S2, B1...

    union
    {
        TipoEvento evento;

        struct // TODO
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
        AcaoRecurso comando; // ACAO_ESTADO
        uint32_t timer;      // ACAO_TIMER
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
void regrasBoot();

int regrasCount();

void regrasGetJSONDoc(JsonDocument &doc);

String regraAtualizaFromJSON(uint8_t *json);

void regrasProcessaEvento(Evento e);

String regrasPersiste();

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

void regraPrint(Regra *r);
