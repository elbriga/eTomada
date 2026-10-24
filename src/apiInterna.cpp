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

bool apiInterna(const char *hostname, IPAddress ip, int port, String endpoint, String metodo, String request, JsonDocument *responseOut, String &msgOut);

bool apiInternaGetSnapshot(const char *hostname, IPAddress ip, JsonDocument &doc, String &msgOut)
{
  return apiInterna(hostname, ip, 80, "getSnapshot", "GET", "", &doc, msgOut);
}

bool apiInternaPing(const char *hostname, IPAddress ip, String &msgOut)
{
  bool pingOK = apiInterna(hostname, ip, 80, "ping", "GET", "", nullptr, msgOut);

  // TODO :: Atualizar todos e remover!
  if (!pingOK && msgOut == "Erro 404") // Considerar os nodos desatualizados sem o "ping"
    pingOK = true;

  return pingOK;
}

bool apiInternaEnviaJSON(const char *hostname, IPAddress ip, int port, String endpoint, String json, String &msgOut)
{
  return apiInterna(hostname, ip, port, endpoint, "POST", json, nullptr, msgOut);
}

bool apiInternaEnviaJSON(const char *hostname, String ipPort, String endpoint, String json, String &msgOut)
{
  int port = 80;
  String ipStr = ipPort;

  const int pos = ipPort.indexOf(':');
  if (pos >= 0)
  {
    String portStr = ipPort.substring(pos + 1);
    int p = portStr.toInt();
    if (p <= 0 || p > 65535)
    {
      msgOut = "Porta invalida";
      return false;
    }

    ipStr = ipPort.substring(0, pos);
    port = p;
  }

  IPAddress ip;
  if (!ip.fromString(ipStr))
  {
    msgOut = "IP invalido";
    return false;
  }

  return apiInternaEnviaJSON(hostname, ip, port, endpoint, json, msgOut);
}

bool apiInterna(const char *hostname, IPAddress ip, int port, String endpoint, String metodo, String request, JsonDocument *responseOut, String &msgOut)
{
  if (!ip)
  {
    msgOut = "apiInterna: abortando, sem IP";
    return false;
  }

  String url = "http://" + ip.toString() + (port != 80 ? ":" + String(port) : "") + "/api/" + endpoint;
  if (endpoint != "ping")
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

  if (code < 0 && endpoint != "ping")
    logaM(LOG_AVISO, "Falha HTTP [%s]: %s (%d)",
          url.c_str(),
          http.errorToString(code).c_str(),
          code);

  if (code == 200)
  {
    WiFiClient *stream = http.getStreamPtr();
    if (stream)
    {
      char *response = (char *)malloc(API_INTERNA_RESPONSE_MAXLEN);
      if (!response)
      {
        http.end();
        msgOut = "apiInterna: sem memoria para response";
        return false;
      }
      response[0] = '\0';

      size_t pos = 0;
      int restante = http.getSize();
      uint32_t ultimoDado = millis();

      while (pos < API_INTERNA_RESPONSE_MAXLEN - 1)
      {
        int disponivel = stream->available();
        if (disponivel > 0)
        {
          size_t tamanho = min(
              (size_t)disponivel,
              (size_t)(API_INTERNA_RESPONSE_MAXLEN - 1 - pos));

          if (restante >= 0 && tamanho > (size_t)restante)
            tamanho = restante;

          if (!tamanho)
            break;

          size_t lido = stream->readBytes(response + pos, tamanho);
          if (!lido)
            break;

          pos += lido;

          if (restante >= 0)
          {
            restante -= lido;

            if (!restante)
              break;
          }

          ultimoDado = millis();
          continue;
        }

        if (restante == 0 || !http.connected())
          break;

        if (millis() - ultimoDado >= API_INTERNA_TIMEOUT)
          break;

        delay(1);
      }
      response[pos] = '\0';

      logaM(LOG_DEBUG0, " >> RESP: %s", response);

      if (responseOut)
        utilLeJson("apiInterna", *responseOut, response);

      free(response);
    }
  }

  http.end();

  if (endpoint != "ping")
    logaM(LOG_DEBUG0, "Duracao HTTP [%s]: [%d ms]", url.c_str(), millis() - inicio);

  bool OK = (code == 200 || code == 204);
  msgOut = responseOut && !(*responseOut)["msg"].isNull()
               ? (*responseOut)["msg"].as<String>()
               : (OK ? "OK" : "Erro " + String(code));
  return OK;
}
