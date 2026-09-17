#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "recurso.h"

struct Mestre
{
    String deviceID;
    IPAddress ip;
};

void mestreInit();
void mestreCheckOnline(); // Remover ??

void mestreLoop();
bool mestreAtivo();
IPAddress mestreGetIP();

void mestreEnviaEvento(Recurso *rec, TipoEvento tipoEvento);
