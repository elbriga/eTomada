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
  JsonDocument request, resposta;

  request["id"] = String(rr->idRemoto);
  request["estado"] = estado;

  int code = apiInterna(rr->nodo->ip, "setRecurso", "PUT", &request, &resposta);
  if (code != 200)
  {
    logaM(LOG_CRITICO, "Erro API Interna: %d", code);
    // TODO ??
  }

  // String out;
  // serializeJson(resposta, out);
  // logaM("ATUALIZAR RECURSO REMOTO com Resposta :::::::: [%s]", out.c_str());

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

int apiInterna(IPAddress ip, String endpoint, String metodo, JsonDocument *request, JsonDocument *response)
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
    // TODO http.getString() é perigoso !!! usar o stream
    String respBody = http.getString();
    logaM(LOG_DEBUG, " >> RESP: %s", respBody.c_str());

    if (response)
      utilLeJson("apiInterna", *response, respBody);
  }

  http.end();

  return code;
}
