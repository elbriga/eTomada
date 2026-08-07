#pragma once

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

void httpServerInit();
void httpEnviaSSE(String msg, String tipo);
