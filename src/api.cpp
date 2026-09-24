#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "http.h"
#include "mestre.h"
#include "regras.h"
#include "wifi.h"
#include "util.h"
#include "ota.h"
#include "apiInterna.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("API", nivel, fmt, ##__VA_ARGS__)

void apiSnapshot(AsyncWebServerRequest *request)
{

  String snapshot = eTomadaGetSnapshotJSON();
  request->send(200, "application/json", snapshot);
  logaRequest(request, "200 OK");
}

void apiSetRecurso(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  bool fromMestre = mestreAtivo() && (request->client()->remoteIP() == mestreGetIP());

  Recurso *rec = nullptr;
  String msg = recursoSetFromJSON(data, rec, !fromMestre);

  JsonDocument resposta;
  resposta["msg"] = (fromMestre ? "SIM MESTRE!:" : "") + msg;
  if (fromMestre && rec)
    resposta["recurso"] = recursoGetJSONDoc(rec);

  String payload;
  serializeJson(resposta, payload);
  request->send(200, "application/json", payload);
  logaRequest(request, "200 " + msg);
}

void apiSetRecursoConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String atzCfgOK = recursoAtualizaConfigFromJSON(data);

  request->send(200, "application/json", "{\"msg\": \"" + atzCfgOK + "\"}");
  logaRequest(request, "200 " + atzCfgOK);
}

void apiEvento(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // TODO :: Enviar 404 se nao achar o recurso do evento
  String atzEventoOK = recursoEventoRecebido(data);

  request->send(200, "application/json", "{\"msg\": \"" + atzEventoOK + "\"}");
  // removido por flood! logaRequest(request, "200 " + atzEventoOK);
}

void apiMock(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // TODO :: Enviar 404 se nao achar o recurso do evento
  String mockOK = eventoMockFromJson(data);

  request->send(200, "application/json", "{\"msg\": \"" + mockOK + "\"}");
  logaRequest(request, "200 " + mockOK);
}

void apiGetFile(AsyncWebServerRequest *request)
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
          "application/json");

  request->send(response);
  logaRequest(request, "200 OK");
}

void apiAddNodo(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String addNodoOK = nodoRemotoAddFromJSON(data);

  request->send(200, "application/json", "{\"msg\": \"" + addNodoOK + "\"}");
  logaRequest(request, "200 " + addNodoOK);
}

void apiDelNodo(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String delNodoOK = nodoRemotoDelFromJSON(data);

  request->send(200, "application/json", "{\"msg\": \"" + delNodoOK + "\"}");
  logaRequest(request, "200 " + delNodoOK);
}

void apiGetNodo(AsyncWebServerRequest *request)
{
  JsonDocument ret;

  if (!request->hasParam("id"))
  {
    ret["msg"] = "Informe o ID";
  }
  else
  {
    String id = request->getParam("id")->value();
    NodoRemoto *nr = nodoRemotoGet(id.c_str());
    if (!nr)
    {
      ret["msg"] = "Nodo Invalido";
    }
    else
    {
      ret["msg"] = "OK";

      JsonDocument snapshot;
      apiInternaGetSnapshot(nr->ip, snapshot);
      ret["nodo"] = snapshot;
    }
  }

  int code = ret["msg"] == "OK" ? 200 : 400;
  String body;
  serializeJson(ret, body);
  request->send(code, "application/json", body);
  logaRequest(request, String(code) + " " + ret["msg"].as<String>());
}

void apiAddRecursoRemoto(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String addRROK = recursoRemotoAddFromJSON(data);

  vTaskDelay(pdMS_TO_TICKS(1500)); // Dar tempo do refresh mDNS

  request->send(200, "application/json", "{\"msg\": \"" + addRROK + "\"}");
  logaRequest(request, "200 " + addRROK);
}

void apiDelRecursoRemoto(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String delRROK = recursoRemotoDelFromJSON(data);

  vTaskDelay(pdMS_TO_TICKS(1500)); // Dar tempo do refresh mDNS

  request->send(200, "application/json", "{\"msg\": \"" + delRROK + "\"}");
  logaRequest(request, "200 " + delRROK);
}

void apiSetRegra(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String atzCfgOK = regraAtualizaFromJSON(data);

  request->send(200, "application/json", "{\"msg\": \"" + atzCfgOK + "\"}");
  logaRequest(request, "200 " + atzCfgOK);
}

void apiDelRegra(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String atzCfgOK = regraDeleteFromJSON(data);

  request->send(200, "application/json", "{\"msg\": \"" + atzCfgOK + "\"}");
  logaRequest(request, "200 " + atzCfgOK);
}

void apiFactoryReset(AsyncWebServerRequest *request)
{
  eTomadaFactoryReset();

  request->send(200, "application/json", R"({"msg":"OK"})");
  logaRequest(request, "200 OK");
}

void apiResetWifiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // TODO :: MAGIC!
  WiFiResetConfig();

  request->send(200, "application/json", R"({"msg":"OK"})");
  logaRequest(request, "200 OK");

  utilRestart("reset WiFi");
}

void apiSetConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  if (utilLeJson("/api/setWiFiConfig", doc, data))
  {
    logaRequest(request, "400 JSON Invalido");
    request->send(400, "application/json", R"({"msg":"JSON Invalido"})");
    return;
  }

  if (!doc["ssid"].isNull())
  {
    String ssid = doc["ssid"] | "";
    bool temPass = !doc["pass"].isNull();
    String pass = doc["pass"] | "";
    doc.clear();

    if (ssid == "")
    {
      logaRequest(request, "400 SSID Invalido");
      request->send(400, "application/json", R"({"msg":"SSID Invalido"})");
      return;
    }

    if (!temPass)
    {
      logaRequest(request, "400 sem PASS");
      request->send(400, "application/json", R"({"msg":"PASS Invalido"})");
      return;
    }

    WiFiSalvaConfig(ssid, pass);

    // TODO :: mudar WiFi sem reiniciar??

    request->send(200, "application/json", R"({"msg":"OK - vou reinicar"})");
    logaRequest(request, "200 OK");

    utilRestart("WiFi Change");
  }
  else if (!doc["mestre"].isNull())
  {
    String mestre = doc["mestre"] | "";

    Preferences prefs;
    prefs.begin("eTomada", false);
    prefs.putString("mestre1", mestre);
    prefs.end();

    request->send(200, "application/json", R"({"msg":"OK"})");
    logaRequest(request, "200 OK");
  }
  else
  {
    request->send(200, "application/json", R"({"msg":"config quem?"})");
    logaRequest(request, "200 OK");
  }
}

void apiCheckWWW(AsyncWebServerRequest *request)
{
  otaChecaWWW();

  request->send(200, "application/json", R"({"msg":"WWW conferido"})");
  logaRequest(request, "200 OK");
}

void roletaTask(void *arg)
{
  eTomadaRoleta();
  vTaskDelete(NULL);
}
void apiRoleta(AsyncWebServerRequest *request)
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
      1);
}

void apiAPRedes(AsyncWebServerRequest *request)
{
  request->send(200, "application/json", WiFiGetScanJSON());
  logaRequest(request, "200 OK");
}

void apiAPSetWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  if (utilLeJson("/api/setWiFiConfig", doc, data))
  {
    logaRequest(request, "400 JSON Invalido");
    request->send(400, "application/json", R"({"msg":"JSON Invalido"})");
    return;
  }

  String ssid = doc["ssid"] | "";
  String pass = doc["pass"] | "";
  doc.clear();

  if (ssid == "")
  {
    logaRequest(request, "400 SSID Invalido");
    request->send(400, "application/json", R"({"msg":"SSID Invalido"})");
    return;
  }

  WiFiSalvaConfig(ssid, pass);

  request->send(200, "application/json", R"({"msg":"OK"})");
  logaRequest(request, "200 OK");

  utilRestart("Novo WiFi!");
}
