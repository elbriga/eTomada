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
};

void botoesInit();
int botoesGetCount();
Botao *botaoGet(int numBotao);
void botaoPrint(Botao *botao);

void botaoLoadFromPrefs(Botao *b, int num, Preferences &prefs);
JsonDocument botaoGetJSONDoc(Botao *s, bool full);

void botoesAtualiza();

String botaoAtualizaConfigFromJSON(Recurso *recurso, JsonDocument doc);
