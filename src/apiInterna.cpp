#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "recurso.h"
#include "util.h"
#include "http.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("APIINT", nivel, fmt, ##__VA_ARGS__)

#define API_INTERNA_TIMEOUT 1000
#define API_INTERNA_RESPONSE_MAXLEN 8192

bool apiInterna(const char *hostname, IPAddress ip, int port, String endpoint, String metodo, String request, String &msgOut);

bool apiInternaEnviaJSON(const char *hostname, IPAddress ip, int port, String endpoint, String json, String &msgOut)
{
  return apiInterna(hostname, ip, port, endpoint, "POST", json, msgOut);
}

bool apiInterna(const char *hostname, IPAddress ip, int port, String endpoint, String metodo, String request, String &msgOut)
{
  if (!ip)
  {
    msgOut = "apiInterna: abortando, sem IP";
    return false;
  }

  String url = "http://" + ip.toString() + (port != 80 ? ":" + String(port) : "") + "/api/" + endpoint;

  logaM(LOG_DEBUG0, "apiInterna: %s [%s] %s", metodo.c_str(), hostname, url.c_str());

  uint32_t inicio = millis();

  HTTPClient http;
  http.begin(url);
  http.setAuthorization(ETOMADA_HTTP_USERNAME, httpGetSenha().c_str());
  http.setConnectTimeout(API_INTERNA_TIMEOUT);
  http.setTimeout(API_INTERNA_TIMEOUT);
  http.setUserAgent("eTomada Full " + eTomadaDeviceID() + " v" + eTomadaGetVersao());

  esp_task_wdt_reset(); // alimenta o watchdog

  int code = 0;
  if (metodo == "PUT" || metodo == "POST")
  {
    String body = "{}";
    if (request != "")
    {
      body = request;
      logaM(LOG_DEBUG0, ">> BODY: %s", body.c_str());
    }

    http.addHeader("Content-Type", "application/json");
    code = (metodo == "PUT") ? http.PUT(body) : http.POST(body);
  }
  else
  {
    code = http.GET();
  }

  esp_task_wdt_reset(); // alimenta o watchdog

  if (code < 0)
    logaM(LOG_AVISO, "Falha HTTP [%s]: %s (%d)",
          url.c_str(),
          http.errorToString(code).c_str(),
          code);

  http.end();

  logaM(LOG_DEBUG0, "Duracao HTTP [%s]: [%d ms]", url.c_str(), millis() - inicio);

  bool OK = (code == 200 || code == 204);
  msgOut = OK ? "OK" : "Erro " + String(code);

  return OK;
}
