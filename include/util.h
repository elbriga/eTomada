#pragma once

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

void utilDIE(const char *msg);
void utilRestart(const char *msg);

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino);

const char *utilGetDiaSemana(struct tm timeinfo);

int utilVersionToInt(const char *ver);

DeserializationError utilLeJson(const char *onde, JsonDocument &doc, uint8_t *str);
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, String jsonStr);
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, File file);
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, WiFiUDP udp);
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, WiFiClient &wifiClient);
