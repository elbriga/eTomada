#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

bool apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc, String &msgOut);
bool apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta, String &msgOut);

bool apiInternaEnviaJSON(IPAddress ip, int port, String endpoint, String json, String &msgOut);
bool apiInternaEnviaJSON(String ipPort, String endpoint, String json, String &msgOut);
