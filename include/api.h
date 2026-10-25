#pragma once
#include <ESPAsyncWebServer.h>

void apiSnapshot(AsyncWebServerRequest *request);
void apiGetRecurso(AsyncWebServerRequest *request);
void apiSetRecurso(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiSetRecursoConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiMock(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiFiles(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiSetRegras(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiFactoryReset(AsyncWebServerRequest *request);
void apiSetConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiCheckWWW(AsyncWebServerRequest *request);
void apiRegrasBoot(AsyncWebServerRequest *request);

void apiAPRedes(AsyncWebServerRequest *request);
void apiAPSetWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
