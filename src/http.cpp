#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "sensor.h"
#include "wifi.h"
#include "discover.h"
#include "recurso.h"

#define DEV // TODO :: remover

// Web Server
AsyncWebServer httpServer(80);
AsyncEventSource sse("/events");

void httpServerInitModoAP();
void httpServerInitModoAPI();
void logaRequest(AsyncWebServerRequest *request, String resultado);

void httpEnviaEvento(String msg, String tipo) {
  sse.send(msg, tipo);
}

void httpServerInit() {
#ifdef DEV
  //  Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
#endif

  if (WiFiGetModoAP()) httpServerInitModoAP();
  else                 httpServerInitModoAPI();

  httpServer.onNotFound([](AsyncWebServerRequest *request) {
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
    }
  });

  httpServer.begin();
}

void roletaTask(void *arg) {
  eTomadaRoleta();
  vTaskDelete(NULL);
}

void httpServerInitModoAPI() {
  httpServer.on("/api/getSnapshot", HTTP_GET, [](AsyncWebServerRequest *request) {
    String body = eTomadaGetSnapshotJSON(true);
    request->send(200, "application/json", body);
    logaRequest(request, "200 OK");
  });

  httpServer.on("/api/discover", HTTP_GET, [](AsyncWebServerRequest *request) {
    String body = "{\"scantime\":3,\"msg\":\"Escaneando...\"}";
    request->send(200, "application/json", body);
    logaRequest(request, "200 OK");

    discoverStart();
  });

  httpServer.on("/api/setRecurso", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String setOK = recursoSetFromJSON(data);
    if (setOK == "OK") {
      request->send(200, "application/json", "{\"msg\": \"OK\"}");
      logaRequest(request, "200 OK");
    } else {
      request->send(400, "application/json", "{\"msg\": \""+setOK+"\"}"); 
      logaRequest(request, "400 " + setOK);
    }
  });

  httpServer.on("/api/setRecursoConfig", HTTP_PUT, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String atzCfgOK = recursoAtualizaConfigFromJSON(data);
    
    request->send(200, "application/json", "{\"msg\": \""+atzCfgOK+"\"}");
    logaRequest(request, "200 " + atzCfgOK);

    processaRegras();
  });

  httpServer.on("/api/factoryReset", HTTP_POST, [](AsyncWebServerRequest *request) {
    eTomadaFactoryReset();
    request->send(200, "application/json", "{\"msg\": \"OK\"}");
    logaRequest(request, "200 OK");
  });
  
  httpServer.on("/api/resetWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    WiFiResetConfig();
    request->send(200, "application/json", "{\"msg\":\"OK\"}");
    logaRequest(request, "200 OK");

    delay(1000);
    ESP.restart();
  });

  httpServer.on("/api/roleta", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    );
  });

  // Eventos de conexão/desconexão
  sse.onConnect([](AsyncEventSourceClient *client) {
    logaMensagem("Cliente SSE conectado de [%s]", client->client()->remoteIP().toString().c_str());

    // Snapshot ao conectar
    String body = eTomadaGetSnapshotJSON(true);
    client->send(body, "sse_snapshot", millis(), 2500);
  });

  httpServer.addHandler(&sse);

  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

void httpServerInitModoAP() {
  // Android
  httpServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
    logaRequest(request, "Redir /");
  });
  // iOS
  httpServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
    logaRequest(request, "Redir /");
  });
  // Windows
  httpServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
    logaRequest(request, "Redir /");
  });

  httpServer.on("/api/redes", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", WiFiGetScanJSON());
    logaRequest(request, "200 OK");
  });

  httpServer.on("/api/setWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;

    DeserializationError err = deserializeJson(doc, data);
    if (err) {
      logaRequest(request, "400 JSON Invalido");
      request->send(400, "application/json", "{\"msg\":\"json invalido\"}");
      return;
    }

    String ssid = doc["ssid"] | "";
    if (ssid == "") {
      logaRequest(request, "400 SSID Invalido");
      request->send(400, "application/json", "{\"msg\":\"ssid invalido\"}");
      return;
    }
    String pass = doc["pass"] | "";

    WiFiSalvaConfig(ssid, pass);

    request->send(200, "application/json", "{\"msg\":\"OK\"}");
    logaRequest(request, "200 OK");

    delay(1000);
    ESP.restart();
  });

  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");
}

void logaRequest(AsyncWebServerRequest *request, String resultado) {
  logaMensagem("[org:%s] %s %s => [%s]",
    request->client()->remoteIP().toString(),
    request->methodToString(),
    request->url().c_str(),
    resultado.c_str());
}
