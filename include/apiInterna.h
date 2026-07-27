#include <ArduinoJson.h>

#include "nodoRemoto.h"

String apiInternaGetSnapshot(NodoRemoto *nodo, JsonDocument &doc);
String apiInternaSetRecurso(NodoRemoto *nodo, String id, String estado);
