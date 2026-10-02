#pragma once
#include <ArduinoJson.h>

#include "nodoRemoto.h"

bool apiInternaGetSnapshot(IPAddress ip, JsonDocument &doc, String &msgOut);
bool apiInternaSetRecurso(IPAddress ip, TipoNodoRemoto tipoNodo, const char *idRemoto, String estado, JsonDocument &resposta, String &msgOut);

String apiInternaEnviaEvento(String ipPort, JsonDocument *body);
String apiInternaEnviaEvento(IPAddress ip, JsonDocument *body, int port = 80);
