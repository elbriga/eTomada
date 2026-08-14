#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "recurso.h"

struct Mestre
{
    String mac;
    IPAddress ip;
    bool online;
    uint32_t ultimoHeartbeat;
};

void mestreInit();
void mestreLoop();
bool mestreAtivo();
IPAddress mestreGetIP();

void mestreCheckDiscover(String mac, IPAddress ip);

void mestreEnviaEvento(Recurso *rec, TipoEvento tipoEvento);
