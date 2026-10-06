#pragma once

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#define ETOMADA_HTTP_USERNAME "admin"
#define ETOMADA_HTTP_DEFAULT_PASSWORD "sapo"

void httpServerInit();
void httpSetSenha(const String &senha);
String httpGetSenha();
IPAddress httpGetClientIP(AsyncWebServerRequest *request);
void httpEnviaSSE(String msg, String tipo);
void httpEnviaSSERefresh();
void logaRequest(AsyncWebServerRequest *request, String resultado);
