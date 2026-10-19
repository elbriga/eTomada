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
#include "mutex.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("API", nivel, fmt, ##__VA_ARGS__)

void apiSnapshot(AsyncWebServerRequest *request)
{
  if (request->hasHeader("CF-Connecting-IP"))
  {
    const AsyncWebHeader *h = request->getHeader("CF-Connecting-IP");
    logaM(LOG_NORMAL, "getSnapshot > IP externo: %s\n", h->value().c_str());
  }

  String snapshot = eTomadaGetSnapshotJSON();
  request->send(200, "application/json", snapshot);
  logaRequest(request, "200 OK");
}

void apiGetRecurso(AsyncWebServerRequest *request)
{
  if (!request->hasParam("id"))
  {
    request->send(400, "application/json", R"({"msg":"Parametro 'id' obrigatorio"})");
    logaRequest(request, "400 Missing id");
    return;
  }

  String recursoID = request->getParam("id")->value();

  JsonDocument doc;
  {
    MutexLock lock(modeloMutex, "apiGetRecurso");
    if (!lock)
      doc["msg"] = "Erro de LOCK!";
    else
    {
      Recurso *r = recursoGet(recursoID.c_str());
      if (!r)
        doc["msg"] = "Recurso Invalido!";
      else
        doc = recursoGetJSONDoc(r);
    }
  }

  String body;
  serializeJson(doc, body);
  request->send(200, "application/json", body);
  // logaRequest(request, "200 OK");
}

void apiSetRecurso(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  bool fromMestre = mestreAtivo() && (request->client()->remoteIP() == mestreGetIP());

  String recursoID;
  String msg;
  if (!recursoSetFromJSON(data, recursoID, msg, !fromMestre))
    logaM(LOG_AVISO, "apiSetRecurso > recursoSetFromJSON > [%s]", msg.c_str());

  if (fromMestre)
    msg = "SIM MESTRE:" + msg;

  JsonDocument resposta;
  resposta["msg"] = msg;
  if (recursoID != "")
  {
    MutexLock lock(modeloMutex, "apiSetRecurso");
    if (lock)
    {
      Recurso *rec = recursoGet(recursoID.c_str());
      resposta["recurso"] = recursoGetJSONDoc(rec);
    }
  }

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
  String atzEventoOK = recursoEventoRecebido(data, request->client()->remoteIP());

  request->send(200, "application/json", "{\"msg\": \"" + atzEventoOK + "\"}");

  if (atzEventoOK != "OK")
    logaRequest(request, "200 " + atzEventoOK);
}

void apiMock(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String mockOK = eventoMockFromJson(data);

  request->send(200, "application/json", "{\"msg\": \"" + mockOK + "\"}");
  logaRequest(request, "200 " + mockOK);
}

void lsRecursivo(File &pasta, JsonArray files, int &tamanho)
{
  File file = pasta.openNextFile();
  while (file)
  {
    if (file.isDirectory())
      lsRecursivo(file, files, tamanho);
    else
    {
      JsonObject item = files.add<JsonObject>();
      item["name"] = file.path();
      item["size"] = file.size();
      tamanho += file.size();
    }

    file.close();
    file = pasta.openNextFile();
  }
}

bool apiFilesValidaCaminho(String &file)
{
  if (file.isEmpty() || file.indexOf("..") >= 0 ||
      file.indexOf('\\') >= 0)
    return false;

  if (!file.startsWith("/"))
    file = "/" + file;

  return file != "/" && !file.endsWith("/");
}

void apiFiles(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // PUT - Upload binario, em blocos
  if (request->hasParam("put"))
  {
    String filePut = request->getParam("put")->value();

    if (!apiFilesValidaCaminho(filePut))
    {
      if (index == 0)
      {
        request->send(400, "application/json", R"({"msg":"Nome de arquivo invalido"})");
        logaRequest(request, "400 PUT Invalid file");
      }
      return;
    }

    // Primeiro bloco: abre arquivo para escrita
    if (index == 0)
    {
      request->_tempFile = LittleFS.open(filePut, "w");

      if (!request->_tempFile)
      {
        request->send(500, "application/json", R"({"msg":"Erro ao abrir arquivo"})");
        logaRequest(request, "500 PUT Open " + filePut);
        return;
      }
    }

    // Abertura ou escrita anterior falhou
    if (!request->_tempFile)
      return;

    // Grava bloco recebido
    if (request->_tempFile.write(data, len) != len)
    {
      request->_tempFile.close();
      LittleFS.remove(filePut);

      request->send(507, "application/json", R"({"msg":"Erro ao gravar arquivo"})");
      logaRequest(request, "507 PUT Write " + filePut);
      return;
    }

    // Ultimo bloco
    if (index + len == total)
    {
      request->_tempFile.close();

      request->send(200, "application/json", R"({"msg":"Arquivo gravado"})");
      logaRequest(request, "200 PUT " + filePut + " - " + String(total) + " bytes");
    }

    return;
  }

  // Operacoes JSON: file, del, ls
  // Acumula o corpo caso chegue dividido em blocos.
  constexpr size_t MAX_JSON = 4096;

  if (total > MAX_JSON)
  {
    if (index == 0)
      request->send(413, "application/json", R"({"msg":"JSON muito grande"})");
    return;
  }

  if (index == 0)
    request->_tempObject = malloc(total + 1);

  if (!request->_tempObject)
  {
    if (index == 0)
      request->send(500, "application/json", R"({"msg":"Memoria insuficiente"})");
    return;
  }

  if (index > total || len > total - index)
  {
    request->send(400, "application/json", R"({"msg":"Corpo HTTP invalido"})");
    return;
  }

  memcpy((uint8_t *)request->_tempObject + index, data, len);

  if (index + len < total)
    return;

  ((char *)request->_tempObject)[total] = '\0';

  JsonDocument doc;
  if (deserializeJson(doc, (char *)request->_tempObject, total))
  {
    logaRequest(request, "400 JSON Invalido");
    request->send(400, "application/json", R"({"msg":"JSON Invalido"})");
    return;
  }

  // GET FILE
  if (!doc["getFile"].isNull())
  {
    String file = doc["getFile"];
    if (!apiFilesValidaCaminho(file))
    {
      request->send(400, "application/json", R"({"msg":"Nome de arquivo invalido"})");
      logaRequest(request, "400 Invalid file");
      return;
    }

    if (!LittleFS.exists(file))
    {
      request->send(404, "application/json", R"({"msg":"FNF"})");
      logaRequest(request, "404 FNF");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(LittleFS, file, "application/octet-stream");
    request->send(response);
    logaRequest(request, "200 FILE " + file);
    return;
  }

  // DELETE
  else if (!doc["del"].isNull())
  {
    String fileDel = doc["del"];
    if (!apiFilesValidaCaminho(fileDel))
    {
      request->send(400, "application/json", R"({"msg":"Nome de arquivo invalido"})");
      logaRequest(request, "400 Invalid file");
      return;
    }

    if (!LittleFS.exists(fileDel))
    {
      request->send(404, "application/json", R"({"msg":"Arquivo nao encontrado"})");
      logaRequest(request, "404 Del " + fileDel);
      return;
    }

    File arquivo = LittleFS.open(fileDel, "r");
    bool isDir = arquivo && arquivo.isDirectory();
    arquivo.close();
    if (isDir)
    {
      request->send(400, "application/json", R"({"msg":"Nao e permitido remover diretorios"})");
      logaRequest(request, "400 Del diretorio " + fileDel);
      return;
    }

    if (!LittleFS.remove(fileDel))
    {
      request->send(500, "application/json", R"({"msg":"Erro ao remover!"})");
      logaRequest(request, "500 Del Err");
      return;
    }

    request->send(200, "application/json", R"({"msg":"Arquivo removido"})");
    logaRequest(request, "200 Del OK " + fileDel);
    return;
  }

  // LS
  else
  {
    File raiz = LittleFS.open("/");

    if (!raiz || !raiz.isDirectory())
    {
      request->send(500, "application/json", R"({"error":"LittleFS indisponivel"})");
      logaRequest(request, "500 LS");
      return;
    }

    JsonDocument docLs;
    docLs["dir"] = "/";
    docLs["size"] = 0;
    docLs["totFiles"] = 0;
    JsonArray files = docLs["files"].to<JsonArray>();

    int tamanho = 0;
    lsRecursivo(raiz, files, tamanho);
    raiz.close();

    docLs["size"] = tamanho;
    docLs["totFiles"] = files.size();

    String json;
    serializeJson(docLs, json);

    request->send(200, "application/json", json);
    logaRequest(request, "200 LS " + String(files.size()) +
                             " files - " + String(tamanho) + " bytes");
    return;
  }
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
    IPAddress ip;
    String id = request->getParam("id")->value();
    {
      MutexLock lock(modeloMutex, "apiGetNodo");
      if (!lock)
      {
        ret["msg"] = "Erro Mutex!";
      }
      else
      {
        NodoRemoto *nr = nodoRemotoGet(id.c_str());
        if (!nr)
        {
          ret["msg"] = "Nodo Invalido";
        }
        else
        {
          ip = nr->ip;
          ret["msg"] = "OK";
        }
      }
    }

    if (ip)
    {
      String msg;
      JsonDocument snapshot;
      if (apiInternaGetSnapshot(ip, snapshot, msg))
        ret["nodo"] = snapshot;
      else
        ret["msg"] = msg;
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

  vTaskDelay(pdMS_TO_TICKS(3000)); // Dar tempo do refresh de Recursos na interface

  request->send(200, "application/json", "{\"msg\": \"" + addRROK + "\"}");
  logaRequest(request, "200 " + addRROK);
}

void apiDelRecursoRemoto(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String delRROK = recursoRemotoDelFromJSON(data);

  vTaskDelay(pdMS_TO_TICKS(3000)); // Dar tempo do refresh de Recursos na interface

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
  // eTomadaFactoryReset();

  request->send(200, "application/json", R"({"msg":"TODO"})");
  logaRequest(request, "200 OK");
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

    if (!temPass)
    {
      logaRequest(request, "400 sem PASS");
      request->send(400, "application/json", R"({"msg":"PASS Invalido"})");
      return;
    }

    WiFiSalvaConfig(ssid, pass);

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

    request->send(200, "application/json", R"({"msg":"Mestre Configurado"})");
    logaRequest(request, "200 OK");
  }
  else if (!doc["logLevel"].isNull())
  {
    int logLevel = doc["logLevel"].as<int>();
    logaChangeLevel(logLevel);

    request->send(200, "application/json", R"({"msg":"logLevel configurado"})");
    logaRequest(request, "200 OK");
  }
  else if (!doc["adminNewPass"].isNull())
  {
    // verificar se é o mestre ou whitelist, para não testar a senha atual
    bool deFora = false;
    String remoteIP = httpGetClientIP(request, &deFora);
    if (deFora || ((!mestreAtivo() || remoteIP != mestreGetIP().toString()) && remoteIP != "10.0.1.100"))
    {
      if (doc["adminPass"].as<String>() != httpGetSenha())
      {
        request->send(400, "application/json", R"({"msg":"Senha atual incorreta"})");
        logaRequest(request, "400 Senha atual incorreta");
        return;
      }
    }

    String newPass = doc["adminNewPass"];
    if (newPass.length() < 4)
    {
      request->send(400, "application/json", R"({"msg":"Nova senha deve ter pelo menos 4 caracteres"})");
      logaRequest(request, "400 Nova senha deve ter pelo menos 4 caracteres");
      return;
    }

    httpSetSenha(newPass);

    request->send(200, "application/json", R"({"msg":"adminPass configurado"})");
    logaRequest(request, "200 adminPass configurado");
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

void apiRegrasBoot(AsyncWebServerRequest *request)
{
  regrasBoot();

  request->send(200, "application/json", R"({"msg":"Boot das Regras"})");
  logaRequest(request, "200 OK");
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
