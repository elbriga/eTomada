#pragma once
#include <ESPAsyncWebServer.h>

void otaInit();

void otaUpload(
    AsyncWebServerRequest *request,
    String filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool final);

void otaUploadHelper(AsyncWebServerRequest *request);
