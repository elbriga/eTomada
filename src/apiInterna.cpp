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
#define API_INTERNA_RESPONSE_MAXLEN 512

int apiInterna(IPAddress ip, String endpoint, String metodo, JsonDocument *request, JsonDocument *response);

String apiInternaGetSnapshot(NodoRemoto *nodo, JsonDocument &doc)
{
  int code = apiInterna(nodo->ip, "getSnapshot", "GET", nullptr, &doc);

  return code == 200 ? "OK" : String(code);
}

String apiInternaSetRecurso(Recurso *recurso, String estado)
{
  if (!recurso->remoto)
  {
    return "Recurso nao Remoto";
  }

  RecursoRemoto *rr = recurso->recursoRemoto;
  NodoRemoto *nodo = rr->nodo;
  JsonDocument resposta;
  int code = 0;

  String estadoStr =
      estado == "1" ||
              estado == "on" ||
              estado == "ON" ||
              estado == "true"
          ? "ON"
          : "OFF";

  switch (nodo->tipo)
  {
  case TIPO_NODO_FULL:
  {
    JsonDocument request;
    request["id"] = String(rr->idRemoto);
    request["estado"] = estadoStr;

    code = apiInterna(rr->nodo->ip, "setRecurso", "PUT", &request, &resposta);
  }
  break;

  case TIPO_NODO_LITE:
  {
    code = apiInterna(rr->nodo->ip, "setRele?estado=" + estadoStr, "GET", nullptr, &resposta);
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

  if (resposta.isNull())
    return "Resposta vazia!";

  String out;
  serializeJson(resposta, out);
  logaM(LOG_AVISO, "ATUALIZAR RECURSO REMOTO com Resposta :::::::: [%s]", out.c_str());

  switch (recurso->tipo)
  {
  case RECURSO_RELE:
    Rele *rele = &rr->rele;
    rele->estado = resposta["recurso"]["device"]["estado"].as<bool>();
    break;
  }

  // TODO localizar a msg para os params locais
  return resposta["msg"].as<String>();
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
