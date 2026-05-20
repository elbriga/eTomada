#pragma once

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

void httpServerInit();
void httpEnviaEvento(String msg, String tipo);
