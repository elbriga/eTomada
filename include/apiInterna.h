#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

bool apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc, String &msgOut);
bool apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta, String &msgOut);

String apiInternaEnviaJSON(IPAddress ip, String endpoint, String json, int port);
String apiInternaEnviaJSON(String ipPort, String endpoint, String json);
