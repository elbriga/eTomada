#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "wifi.h"

#define DEV // TODO :: remover

String getPortalHtml();

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
#ifdef DEV
  //  Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
#endif

  if ((WiFi.getMode() != WIFI_AP)) {
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

    httpServer.on("/api/factoryReset", HTTP_POST, [](AsyncWebServerRequest *request) {
      eTomadaFactoryReset();
      logaRequest(request, "200 OK");
      request->send(200, "application/json", "{\"msg\": \"OK\"}");
    });
    
    httpServer.on("/api/resetWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {
      WiFiResetConfig();
      logaRequest(request, "200 OK");
      request->send(200, "application/json", "{\"msg\":\"OK\"}");

      delay(1000);
      ESP.restart();
    });

    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  } else {
    // Android
    httpServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
      logaRequest(request, "Redir /");
      request->redirect("/");
    });
    // iOS
    httpServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      logaRequest(request, "Redir /");
      request->redirect("/");
    });
    // Windows
    httpServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
      logaRequest(request, "Redir /");
      request->redirect("/");
    });

    httpServer.on("/api/redes", HTTP_GET, [](AsyncWebServerRequest *request) {
      int totRedes = WiFi.scanNetworks();
      String redes = "[\n";
      for (int i = 0; i < totRedes; i++) {
        String ssid = WiFi.SSID(i);
        if (!ssid.length()) {
          continue;
        }

        //redes += "<option value=\"" + ssid + "\">" + ssid + " (" + WiFi.RSSI(i) + " dBm)</option>";

        if (i > 0) {
          redes += ",\n";
        }
        redes += "\"" + ssid + "\"";
      }
      redes += "\n]";

      logaRequest(request, "200 OK");
      request->send(200, "application/json", redes);
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

      logaRequest(request, "200 OK");
      request->send(200, "application/json", "{\"msg\":\"OK\"}");

      delay(1000);
      ESP.restart();
    });

    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");
  }
  

  httpServer.onNotFound([](AsyncWebServerRequest *request) {
    if ((WiFi.getMode() == WIFI_AP)) {
      logaRequest(request, "Redir /");
      request->redirect("/");
      return;
    }

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
