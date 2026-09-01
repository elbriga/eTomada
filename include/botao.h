#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Recurso; // Forward declaration

#define MAX_BOTOES 4

#define BOTAO_DEBOUCE_TIME_MS 33
#define BOTAO_TEMPO_CLICK_MS 333
#define BOTAO_TEMPO_LONGP_MS 3333
#define BOTAO_TEMPO_BIGP_MS 7777

struct Botao
{
    int num;
    int pino;
    bool ativo;

    bool estado; // nível atual
    bool ultimoEstado;
    uint32_t debounce;
    uint32_t ultimoToggle; // para detectar CLICK
};

void botoesInit();
int botoesGetCount();
Botao *botaoGet(int numBotao);
void botaoPrint(Botao *botao);

JsonDocument botaoGetJSONDoc(Botao *s, bool full);

void botoesAtualiza();
