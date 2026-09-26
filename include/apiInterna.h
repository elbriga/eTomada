#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

String apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc);
String apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta);
String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body);
