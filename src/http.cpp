#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "rele.h"
#include "regras.h"
#include "sensor.h"
#include "wifi.h"
#include "discover.h"
#include "recurso.h"
#include "mestre.h"
#include "eventos.h"
#include "umidificador.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("HTTP", nivel, fmt, ##__VA_ARGS__)

#define DEV // TODO :: remover

// Web Server
AsyncWebServer httpServer(80);
AsyncEventSource sse("/events");

void httpServerInitModoAP();
void httpServerInitModoAPI();
void logaRequest(AsyncWebServerRequest *request, String resultado);

void httpEnviaSSE(String msg, String tipo)
{
  sse.send(msg, tipo);
}

void httpServerInit()
{
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

void roletaTask(void *arg)
{
  eTomadaRoleta();
  vTaskDelete(NULL);
}

void httpServerInitModoAPI()
{
  httpServer.on("/api/getSnapshot", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    String body = eTomadaGetSnapshotJSON();
    request->send(200, "application/json", body);
    logaRequest(request, "200 OK"); });

  httpServer.on("/api/discover", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->send(200, "application/json", R"({"scantime":3,"msg":"Escaneando..."})");
    logaRequest(request, "200 OK");

    discoverStart(false); });

  httpServer.on("/api/setRecurso", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
    bool fromMestre = mestreAtivo() && (request->client()->remoteIP() == mestreGetIP());
    Recurso *rec = nullptr;
    String msg = recursoSetFromJSON(data, rec, !fromMestre);

    JsonDocument resposta;
    resposta["msg"] = (fromMestre ? "SIM MESTRE:": "") + msg;
    if (rec)
      resposta["recurso"] = recursoGetJSONDoc(rec);
    String payload;
    serializeJson(resposta, payload);
    request->send(200, "application/json", payload); 

    logaRequest(request, "200 " + msg); });

  httpServer.on("/api/setRecursoConfig", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                  String atzCfgOK = recursoAtualizaConfigFromJSON(data);

                  request->send(200, "application/json", "{\"msg\": \"" + atzCfgOK + "\"}");
                  logaRequest(request, "200 " + atzCfgOK); });

  httpServer.on("/api/evento", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                  // TODO :: Enviar 404 se nao achar o recurso do evento
                  String atzEventoOK = recursoEventoRecebido(data);

                  request->send(200, "application/json", "{\"msg\": \"" + atzEventoOK + "\"}");
                  // removido por flood! logaRequest(request, "200 " + atzEventoOK);
                });

  httpServer.on("/api/mock", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
    // TODO :: Enviar 404 se nao achar o recurso do evento
    String mockOK = eventoMockFromJson(data);

    request->send(200, "application/json", "{\"msg\": \""+mockOK+"\"}");
    logaRequest(request, "200 " + mockOK); });

  httpServer.on("/api/getFile", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    if (!request->hasParam("file"))
    {
        request->send(400, "application/json", R"({"msg":"Parametro 'file' obrigatorio"})");
        logaRequest(request, "400 Missing file");
        return;
    }

    String file = request->getParam("file")->value();

    if (file.indexOf("..") >= 0)
    {
        request->send(400, "application/json", R"({"msg":"Nome de arquivo invalido"})");
        logaRequest(request, "400 Invalid file");
        return;
    }

    // Garante que o caminho comece com /
    if (!file.startsWith("/"))
        file = "/" + file;

    if (!LittleFS.exists(file))
    {
        request->send(404, "application/json", R"({"msg":"FNF"})");
        logaRequest(request, "404 FNF");
        return;
    }

    AsyncWebServerResponse *response =
        request->beginResponse(
            LittleFS,
            file,
            "application/json"
        );

    request->send(response);

    logaRequest(request, "200 OK"); });

  httpServer.on("/api/setRegra", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                  String atzCfgOK = regraAtualizaFromJSON(data);

                  request->send(200, "application/json", "{\"msg\": \"" + atzCfgOK + "\"}");
                  logaRequest(request, "200 " + atzCfgOK); });

  if (umidificadorAtivo())
    httpServer.on("/api/setUmidificador", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                  {
                  String atzUmidOK = umidificadorSetFromJSON(data);

                  request->send(200, "application/json", "{\"msg\": \"" + atzUmidOK + "\"}");
                  logaRequest(request, "200 " + atzUmidOK); });

  httpServer.on("/api/factoryReset", HTTP_POST, [](AsyncWebServerRequest *request)
                {
    eTomadaFactoryReset();
    request->send(200, "application/json", R"({"msg":"OK"})");
    logaRequest(request, "200 OK"); });

  httpServer.on("/api/resetWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request)
                {
    WiFiResetConfig();
    request->send(200, "application/json", R"({"msg":"OK"})");
    logaRequest(request, "200 OK");

    delay(1000);
    ESP.restart(); });

  httpServer.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request)
                {
    request->send(200, "application/json", R"({"msg":"OK"})");
    logaRequest(request, "200 OK");

    delay(1000);
    ESP.restart(); });

  httpServer.on("/api/roleta", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    String body = "Sorteando!";
    request->send(200, "application/json", body);
    logaRequest(request, "200 OK");
    
    xTaskCreatePinnedToCore(
      roletaTask,
      "roleta",
      4096,
      NULL,
      1,
      NULL,
      1
    ); });

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
  // Android
  httpServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->redirect("/");
    logaRequest(request, "Redir /"); });
  // iOS
  httpServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->redirect("/");
    logaRequest(request, "Redir /"); });
  // Windows
  httpServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->redirect("/");
    logaRequest(request, "Redir /"); });

  httpServer.on("/api/redes", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->send(200, "application/json", WiFiGetScanJSON());
    logaRequest(request, "200 OK"); });

  httpServer.on("/api/setWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
    JsonDocument doc;
    if (utilLeJson("/api/setWiFiConfig", doc, data)) {
      logaRequest(request, "400 JSON Invalido");
      request->send(400, "application/json", R"({"msg":"JSON Invalido"})");
      return;
    }

    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    doc.clear();

    if (ssid == "") {
      logaRequest(request, "400 SSID Invalido");
      request->send(400, "application/json", R"({"msg":"SSID Invalido"})");
      return;
    }

    WiFiSalvaConfig(ssid, pass);

    request->send(200, "application/json", R"({"msg":"OK"})");
    logaRequest(request, "200 OK");

    delay(1000);
    ESP.restart(); });

  httpServer.serveStatic("/", LittleFS, "/www/").setDefaultFile("portal.html");
}

void logaRequest(AsyncWebServerRequest *request, String resultado)
{
  logaM(LOG_NORMAL, "[org:%s] %s %s => [%s]",
        request->client()->remoteIP().toString(),
        request->methodToString(),
        request->url().c_str(),
        resultado.c_str());
}
