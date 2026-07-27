#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"

int apiInterna(NodoRemoto *nodo, String endpoint, String metodo, JsonDocument *request, JsonDocument *response);

String apiInternaGetSnapshot(NodoRemoto *nodo, JsonDocument &doc) {
  int code = apiInterna(nodo, "getSnapshot", "GET", nullptr, &doc);

  return code == 200 ? "OK" : String(code);
}

String apiInternaSetRecurso(NodoRemoto *nodo, String id, String estado) {
  JsonDocument request, response;

  request["id"]     = id;
  request["estado"] = estado;

  int code = apiInterna(nodo, "setRecurso", "PUT", &request, &response);

  String msg = response["msg"];
  return msg;
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
