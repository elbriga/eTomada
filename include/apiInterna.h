#pragma once
#include <ArduinoJson.h>

bool apiInternaEnviaJSON(const char *hostname, IPAddress ip, int port, String endpoint, String json, String &msgOut);
