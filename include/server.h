#pragma once
#include "eventos.h"

void serverInit();

IPAddress serverGetIP();
String serverGetIPPortStr();

void serverEnviaEvento(String payloadJSON);
void serverEnviaSnapshot();
