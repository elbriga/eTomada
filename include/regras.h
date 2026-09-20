#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "eventos.h"
#include "tipoRecurso.h"

#define REGRAS_PATH "/automacoes.json"
#define REGRAS_PATH_DEFAULT "/config/automacoesDefault.json"

#define REGRAS_MAX_CONDICOES 3

enum TipoCondicao
{
    COND_NENHUMA = 0,
    COND_EVENTO = 10,
    COND_HORARIO = 20,
};

enum TipoAcao
{
    ACAO_ESTADO = 10,
    ACAO_TIMER = 20,
    ACAO_DELAY = 30, // TODO
    ACAO_SCRIPT = 40 // TODO
};

struct Condicao
{
    TipoCondicao tipo;
    union
    {
        struct
        {
            TipoEvento tipo;
            char recursoID[32]; // R1, S2, B1...
        } evento;

        struct
        {
            uint8_t hora;
            uint8_t minuto;
        } horario;
    };
    struct
    {
        char variavel[32]; // R1, S2, B1, HORASSECO...
        char op[3];        // >, <, =, !=, >=, <=
        int valor;
    } check;
};

struct Acao
{
    TipoAcao tipo;
    char recursoID[32];
    String comando; // ACAO_ESTADO
    uint32_t timer; // ACAO_TIMER
};

struct Regra
{
    uint16_t id;
    char nome[64];
    bool ativa;
    bool remover;

    Condicao condicao;
    Acao acao;
};

void regrasInit();
void regrasBoot();

int regrasCount();

void regrasGetJSONDoc(JsonDocument &doc, Regra *novaRegra = nullptr);

String regraAtualizaFromJSON(uint8_t *json);
String regraDeleteFromJSON(uint8_t *json);

void regrasProcessaEvento(Evento e);

String regraValida(String regra);

void regraPrint(Regra *r);
