#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "recursoRemoto.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("APIINT", nivel, fmt, ##__VA_ARGS__)

#define API_INTERNA_TIMEOUT 1000
#define API_INTERNA_RESPONSE_MAXLEN 8192

int apiInterna(IPAddress ip, String endpoint, String metodo, JsonDocument *request, JsonDocument *response);

String apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc)
{
  int code = apiInterna(ip, "getSnapshot", "GET", nullptr, &doc);

  return code == 200 ? "OK" : String(code);
}

String apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta)
{
  int code = 0;

  switch (tipoNodo)
  {
  case TIPO_NODO_FULL:
  {
    JsonDocument request;
    request["id"] = idRemoto;
    request["estado"] = estado;

    code = apiInterna(ip, "setRecurso", "PUT", &request, &resposta);
  }
  break;

  case TIPO_NODO_LITE:
  {
    code = apiInterna(ip, "setRele?estado=" + estado, "GET", nullptr, &resposta);
  }
  break;

  default:
    return "Nodo não inicializado!";
  }

  if (code != 200)
  {
    logaM(LOG_CRITICO, "Erro API Interna: %d", code);
    // TODO ??
  }

  // TODO localizar a msg para os params locais
  return "API:" + resposta["msg"].as<String>();
}

String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body)
{
  int code = apiInterna(ip, "evento", "POST", body, nullptr);

  return code == 200 ? "OK" : String(code);
}

int apiInterna(IPAddress ip, String endpoint, String metodo, JsonDocument *request, JsonDocument *responseOut)
{
  String url = "http://" + ip.toString() + "/api/" + endpoint;

  logaM(LOG_DEBUG0, "apiInterna: Acionando %s", url.c_str());

  HTTPClient http;
  http.begin(url);
  http.setTimeout(API_INTERNA_TIMEOUT);

  esp_task_wdt_reset(); // alimenta o watchdog

  int code = 0;
  if (metodo == "PUT" || metodo == "POST")
  {
    String body = "{}";
    if (request != nullptr)
    {
      serializeJson(*request, body);
      logaM(LOG_DEBUG, ">> BODY: %s", body.c_str());
    }

    http.addHeader("Content-Type", "application/json");
    code = (metodo == "PUT") ? http.PUT(body) : http.POST(body);
  }
  else
  {
    code = http.GET();
  }

  esp_task_wdt_reset(); // alimenta o watchdog

  if (code == 200)
  {
    char response[API_INTERNA_RESPONSE_MAXLEN] = {0};

    WiFiClient *stream = http.getStreamPtr();
    if (stream)
    {
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

      logaM(LOG_DEBUG, " >> RESP: %s", response);

      if (responseOut)
        utilLeJson("apiInterna", *responseOut, response);
    }
  }

  http.end();

  return code;
}
