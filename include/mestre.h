#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "recurso.h"

struct Mestre
{
    String deviceID;
    IPAddress ip;
    bool online;
    uint32_t ultimoHeartbeat;
};

void mestreInit();
void mestreCheckOnline();

void mestreLoop();
bool mestreAtivo();
IPAddress mestreGetIP();

void mestreEnviaEvento(Recurso *rec, TipoEvento tipoEvento);
