#pragma once
#include "eventos.h"

#define ETOMADA_SERVER "10.0.0.1:8080"

void serverInit();
void serverEnviaBoot();

IPAddress serverGetIP();

void serverEnviaEvento(String payloadJSON);
void serverEnviaSnapshot();
