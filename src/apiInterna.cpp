#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "recursoRemoto.h"

int apiInterna(NodoRemoto *nodo, String endpoint, String metodo, JsonDocument *request, JsonDocument *response);

String apiInternaGetSnapshot(NodoRemoto *nodo, JsonDocument &doc) {
  int code = apiInterna(nodo, "getSnapshot", "GET", nullptr, &doc);

  return code == 200 ? "OK" : String(code);
}

String apiInternaSetRecurso(Recurso *recurso, String estado) {
  if (!recurso->remoto) {
    return "Recurso nao Remoto";
  }

  RecursoRemoto *rr = recurso->recursoRemoto;
  NodoRemoto    *nr = rr->nodo;
  String         id = String("R") + String(rr->num);
  
  JsonDocument request, resposta;
  request["id"]     = id;
  request["estado"] = estado;
  
  int code = apiInterna(nr, "setRecurso", "PUT", &request, &resposta);
  if (code != 200) {
    logaMensagem("Erro API Interna: %d", code);
    // TODO ??
  }

  String out;
  serializeJson(resposta, out);
  logaMensagem("ATUALIZAR RECURSO REMOTO com Resposta :::::::: [%s]", out.c_str());

  switch (recurso->tipo)
  {
  case RECURSO_RELE:
    Rele *rele = &rr->rele;
    rele->estado   = resposta["recurso"]["device"]["estado"].as<bool>();
    rele->override = resposta["recurso"]["device"]["override"].as<int>();
    break;
  }

  // TODO localizar a msg para os params locais
  return resposta["msg"].as<String>();
}

int apiInterna(NodoRemoto *nodo, String endpoint, String metodo, JsonDocument *request, JsonDocument *response) {
  String url = "http://" + nodo->ip.toString() + "/api/" + endpoint;

  logaMensagem("apiInterna: Acionando %s", url.c_str());

  HTTPClient http;
  http.begin(url);

  int code = 0;
  if (metodo == "PUT") {
    String body = "{}";
    if (request != nullptr) {
      serializeJson(*request, body);
    }

    http.addHeader("Content-Type", "application/json");
    code = http.PUT(body);
  } else {
    code = http.GET();
  }

  if (response && code == HTTP_CODE_OK) {
    deserializeJson(*response, http.getStream());
  }

  http.end();

  return code;
}
