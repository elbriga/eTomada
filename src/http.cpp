#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "wifi.h"

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
  if ((WiFi.getMode() == WIFI_AP)) {
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html; charset=utf-8", getPortalHtml());
    });

    // Android
    httpServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    // iOS
    httpServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    // Windows
    httpServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });

    httpServer.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;

      DeserializationError err = deserializeJson(doc, data);
      if (err) {
        request->send(400, "application/json", "{\"msg\":\"json invalido\"}");
        return;
      }

      String ssid = doc["ssid"] | "";
      if (ssid == "") {
        request->send(400, "application/json", "{\"msg\":\"ssid invalido\"}");
        return;
      }
      String pass = doc["pass"] | "";

      WiFiSalvaConfig(ssid, pass);

      request->send(200, "application/json", "{\"msg\":\"OK\"}");

      delay(1000);
      ESP.restart();
    });
  } else {
    //  Adicionar headers para functionar o CORS quando em DEV localhost
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");

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
  }
  
  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  httpServer.onNotFound([](AsyncWebServerRequest *request) {
    if ((WiFi.getMode() == WIFI_AP)) {
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

String getPortalHtml()
{
  int totRedes = WiFi.scanNetworks();
  String options = "";
  for (int i = 0; i < totRedes; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) {
      continue;
    }

    options += "<option value=\"" + ssid + "\">" + ssid + " (" + WiFi.RSSI(i) + " dBm)</option>";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>eTomada Setup</title>
    <style>
body {
  background: #0f172a;
  color: white;
  font-family: Arial;
  padding: 20px;
}

.card {
  background: #1e293b;
  border-radius: 10px;
  padding: 20px;
  max-width: 400px;
  margin: auto;
}

input, select, button {
  width: 100%;
  padding: 12px;
  margin-top: 10px;
  border-radius: 6px;
  border: none;
  box-sizing: border-box;
}

button {
  background: #2563eb;
  color: white;
  font-weight: bold;
}

.small {
  opacity: 0.7;
  font-size: 12px;
}
    </style>

    <script>
async function salvar() {
  const status = document.getElementById("status");

  const ssid = document.getElementById("ssid").value;
  if (!ssid) {
    alert("Selecione uma rede");
    return;
  }  
  const pass = document.getElementById("pass").value;

  status.innerHTML = "Salvando...";
  try {
    const res = await fetch("/api/wifi", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        ssid,
        pass
      })
    });

    if (!res.ok) {
      throw new Error();
    }

    status.innerHTML = "Configuração salva.<br>Reiniciando dispositivo...";
  } catch(e) {
    status.innerHTML = "Erro ao salvar configuração";
  }
}
  </script>
  </head>
  <body>
    <div class="card">
      <h2>⚡ eTomada Setup</h2>
      <div class="small">
        Configure sua rede WiFi
      </div>
      <br>
      <select id="ssid">
        <option value="">Selecione uma rede</option>)rawliteral";

  html += options;

  html += R"rawliteral(
      </select>
      <input id="pass" type="password" placeholder="Senha WiFi">

      <button onclick="salvar()">Conectar</button>
      <br><br>

      <div id="status"></div>
    </div>
  </body>
</html>
)rawliteral";

  return html;
}
