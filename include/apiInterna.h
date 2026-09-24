#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

String apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc);
String apiInternaSetRecurso(Recurso *recurso, String estado);
String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body);
