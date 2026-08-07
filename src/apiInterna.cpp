#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "recursoRemoto.h"

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
    logaMensagem("Erro API Interna: %d", code);
    // TODO ??
  }

  // String out;
  // serializeJson(resposta, out);
  // logaMensagem("ATUALIZAR RECURSO REMOTO com Resposta :::::::: [%s]", out.c_str());

  switch (recurso->tipo)
  {
  case RECURSO_RELE:
    Rele *rele = &rr->rele;
    rele->estado = resposta["recurso"]["device"]["estado"].as<bool>();
    rele->override = resposta["recurso"]["device"]["override"].as<int>();
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

  logaMensagem("apiInterna: Acionando %s", url.c_str());

  HTTPClient http;
  http.begin(url);

  int code = 0;
  if (metodo == "PUT" || metodo == "POST")
  {
    String body = "{}";
    if (request != nullptr)
    {
      serializeJson(*request, body);
      logaMensagem(">> BODY: %s", body.c_str());
    }

    http.addHeader("Content-Type", "application/json");
    code = (metodo == "PUT") ? http.PUT(body) : http.POST(body);
  }
  else
  {
    code = http.GET();
  }

  // TODO http.getString() é perigoso !!! usar o stream
  String respBody = http.getString();
  logaMensagem(" >> RESP: %s", respBody.c_str());

  if (response)
    deserializeJson(*response, respBody);

  http.end();

  return code;
}
