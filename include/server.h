#pragma once
#include "eventos.h"

void serverInit();
void serverEnviaBoot();

IPAddress serverGetIP();
String serverGetIPPortStr();

void serverEnviaEvento(String payloadJSON);
void serverEnviaSnapshot();
