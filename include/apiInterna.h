#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

bool apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc, String &msgOut);
bool apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta, String &msgOut);
String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body);
