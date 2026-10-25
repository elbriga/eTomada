#pragma once
#include <ESPAsyncWebServer.h>

struct Recurso; // Forward declaration

struct Microfone
{
    bool ligado;
};

void micInit();
bool micAtivo();

Microfone *micGet(); // Somente 1 por eTomada

JsonDocument micGetJSONDoc(Recurso *r, bool full);

void apiMicRecord(AsyncWebServerRequest *request);
