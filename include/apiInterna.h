#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

String apiInternaGetSnapshot(NodoRemoto *nodo, JsonDocument &doc);
String apiInternaSetRecurso(Recurso *recurso, String estado);
String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body);
