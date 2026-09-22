#include <ESPAsyncWebServer.h>

void apiSnapshot(AsyncWebServerRequest *request);
void apiSetRecurso(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiSetRecursoConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiEvento(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiMock(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiGetFile(AsyncWebServerRequest *request);
void apiGetNodo(AsyncWebServerRequest *request);
void apiSetRegra(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiDelRegra(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiFactoryReset(AsyncWebServerRequest *request);
void apiResetWifiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiSetConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void apiCheckWWW(AsyncWebServerRequest *request);
void apiRoleta(AsyncWebServerRequest *request);

void apiAPRedes(AsyncWebServerRequest *request);
void apiAPSetWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
