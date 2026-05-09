#pragma once
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

typedef void (*HttpConfigChangedCallback)();

void httpServerInit(HttpConfigChangedCallback cb);
