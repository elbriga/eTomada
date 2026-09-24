#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "wifi.h"
#include "recovery.h"
#include "api.h"

#define DEV // TODO :: remover

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("HTTP", nivel, fmt, ##__VA_ARGS__)

// Web Server
AsyncWebServer httpServer(80);
AsyncEventSource sse("/events");

void httpServerInitModoAP();
void httpServerInitModoAPI();

void httpServerInit()
{
  logaM(LOG_NORMAL, "Inicializando o servidor http");

#ifdef DEV
  //  Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
#endif

  if (WiFiGetModoAP())
    httpServerInitModoAP();
  else
    httpServerInitModoAPI();

  httpServer.onNotFound([](AsyncWebServerRequest *request)
                        {
    if (WiFiGetModoAP()) {
      logaRequest(request, "Redir /");
      request->redirect("/");
      return;
    }

    // Tratar o OPTIONS
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
      logaRequest(request, "OPTIONS");
    } else {
      request->send(404);
      logaRequest(request, "404 Not Found");
    } });

  httpServer.begin();
}

void httpServerInitModoAPI()
{
  ArRequestHandlerFunction funcVazia = [](AsyncWebServerRequest *request) {};

  httpServer.on("/api/getSnapshot", HTTP_GET, apiSnapshot);
  httpServer.on("/api/setRecurso", HTTP_PUT, funcVazia, NULL, apiSetRecurso);
  httpServer.on("/api/setRecursoConfig", HTTP_PUT, funcVazia, NULL, apiSetRecursoConfig);
  httpServer.on("/api/evento", HTTP_POST, funcVazia, NULL, apiEvento);
  httpServer.on("/api/mock", HTTP_POST, funcVazia, NULL, apiMock);
  httpServer.on("/api/getFile", HTTP_GET, apiGetFile);
  httpServer.on("/api/getNodo", HTTP_GET, apiGetNodo);
  httpServer.on("/api/addNodo", HTTP_PUT, funcVazia, NULL, apiAddNodo);
  httpServer.on("/api/delNodo", HTTP_PUT, funcVazia, NULL, apiDelNodo);
  httpServer.on("/api/addRecursoRemoto", HTTP_PUT, funcVazia, NULL, apiAddRecursoRemoto);
  httpServer.on("/api/delRecursoRemoto", HTTP_PUT, funcVazia, NULL, apiDelRecursoRemoto);
  httpServer.on("/api/setRegra", HTTP_PUT, funcVazia, NULL, apiSetRegra);
  httpServer.on("/api/delRegra", HTTP_PUT, funcVazia, NULL, apiDelRegra);
  httpServer.on("/api/factoryReset", HTTP_POST, apiFactoryReset);
  httpServer.on("/api/resetWiFiConfig", HTTP_POST, funcVazia, NULL, apiResetWifiConfig);
  httpServer.on("/api/setConfig", HTTP_POST, funcVazia, NULL, apiSetConfig);
  httpServer.on("/api/checkWWW", HTTP_GET, apiCheckWWW);
  httpServer.on("/api/roleta", HTTP_GET, apiRoleta);

  recoveryAPIRegister();

  // Eventos de conexão/desconexão
  sse.onConnect([](AsyncEventSourceClient *client)
                {
    logaM(LOG_NORMAL, "Cliente SSE conectado de [%s]", client->client()->remoteIP().toString().c_str());

    // Snapshot ao conectar
    String body = eTomadaGetSnapshotJSON();
    client->send(body, "sse_snapshot", millis(), 2500); });

  httpServer.addHandler(&sse);

  httpServer.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
}

void httpServerInitModoAP()
{
  ArRequestHandlerFunction funcRedir = [](AsyncWebServerRequest *request)
  {
    request->redirect("/");
    logaRequest(request, "Redir /");
  };

  httpServer.on("/generate_204", HTTP_GET, funcRedir);        // Android
  httpServer.on("/hotspot-detect.html", HTTP_GET, funcRedir); // iOS
  httpServer.on("/connecttest.txt", HTTP_GET, funcRedir);     // Windows

  httpServer.on("/api/redes", HTTP_GET, apiAPRedes);
  httpServer.on("/api/setWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, apiAPSetWiFiConfig);

  httpServer.serveStatic("/", LittleFS, "/www/").setDefaultFile("portal.html");
}

void httpEnviaSSE(String msg, String tipo)
{
  sse.send(msg, tipo);
}

void httpEnviaSSERefresh()
{
  String body = eTomadaGetSnapshotJSON();
  httpEnviaSSE(body, "sse_snapshot");
}

void logaRequest(AsyncWebServerRequest *request, String resultado)
{
  logaM(LOG_NORMAL, "[org:%s] %s %s => [%s]",
        request->client()->remoteIP().toString(),
        request->methodToString(),
        request->url().c_str(),
        resultado.c_str());
}
