#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct Recurso; // Forward declaration

#define MAX_BOTOES 4

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
