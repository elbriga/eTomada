#pragma once
#include <ESPAsyncWebServer.h>

struct Recurso; // Forward declaration

struct Microfone
{
    bool ligado;
};

enum class MicAtivacaoResultado
{
    OK,
    INDISPONIVEL,
    OCUPADO,
    SEM_MEMORIA,
    ERRO_TASK
};

void micInit();
bool micAtivo();

MicAtivacaoResultado micAtivar();
void apiMicActivate(AsyncWebServerRequest *request);

Microfone *micGet(); // Somente 1 por eTomada

JsonDocument micGetJSONDoc(Recurso *r, bool full);

void apiMicRecord(AsyncWebServerRequest *request);
