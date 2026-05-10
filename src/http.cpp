#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"

// Web Server
AsyncWebServer httpServer(80);

void logaRequest(AsyncWebServerRequest *request, String resultado)
{
  logaMensagem("[org:%s] %s %s => [%s]",
    request->client()->remoteIP().toString(),
    request->methodToString(),
    request->url().c_str(),
    resultado.c_str());
}

void httpServerInit()
{
  // #ifdef DEV   TODO
  //  Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  // #endif

  httpServer.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = eTomadaGetDataJSON();
    logaRequest(request, "200 OK");
    request->send(200, "application/json", out);
  });

  httpServer.on("/api/setReleConfig", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String atzCfgOK = relesAtualizaConfigFromJSON(data);
    if (atzCfgOK == "OK") {
      processaRegras();
      
      logaRequest(request, "200 OK");
      request->send(200, "application/json", "{\"msg\": \"OK\"}");
    } else {
      logaRequest(request, "400 " + atzCfgOK);
      request->send(400, "application/json", "{\"msg\": \""+atzCfgOK+"\"}"); 
    }
  });

  httpServer.on("/api/factoryReset", HTTP_GET, [](AsyncWebServerRequest *request) {
    eTomadaFactoryReset();
    logaRequest(request, "200 OK");
    request->send(200, "application/json", "{\"msg\": \"OK\"}");
  });

  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  httpServer.onNotFound([](AsyncWebServerRequest *request) {
    // Tratar o OPTIONS
    if (request->method() == HTTP_OPTIONS) {
      logaRequest(request, "OPTIONS");
      request->send(200);
    } else {
      logaRequest(request, "404 Not Found");
      request->send(404);
    }
  });

  httpServer.begin();
}
