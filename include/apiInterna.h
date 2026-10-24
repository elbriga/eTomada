#pragma once
#include <ArduinoJson.h>

bool apiInternaGetSnapshot(const char *hostname, IPAddress ip, JsonDocument &doc, String &msgOut);

bool apiInternaPing(const char *hostname, IPAddress ip, String &msgOut);

bool apiInternaEnviaJSON(const char *hostname, IPAddress ip, int port, String endpoint, String json, String &msgOut);
bool apiInternaEnviaJSON(const char *hostname, String ipPort, String endpoint, String json, String &msgOut);
