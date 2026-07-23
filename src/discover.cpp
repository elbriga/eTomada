#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "eTomada.h"
#include "loga.h"

#define DISCOVER_PORT 8266

static WiFiUDP discoverUdp;

void discoverTask(void *arg);
static bool discoverTaskRunning = false;

void discoverInit() {
    discoverUdp.begin(DISCOVER_PORT);
}

void discoverLoop() {
    if (discoverTaskRunning) {
        return;
    }

    int len = discoverUdp.parsePacket();

    if (len <= 0)
        return;

    // Protocolo
    JsonDocument doc;
    deserializeJson(doc, discoverUdp);
    if (doc["cmd"] != "discover")
        return;

    vTaskDelay(pdMS_TO_TICKS(20));

    logaMensagem("Respondendo ao DISCOVER para %s:%d",
        discoverUdp.remoteIP().toString().c_str(),
        discoverUdp.remotePort() + 1);

    // RESPONDER ==
    discoverUdp.beginPacket(
        discoverUdp.remoteIP(),
        discoverUdp.remotePort() + 1
    );
    discoverUdp.print(eTomadaGetSnapshotJSON(false));
    discoverUdp.endPacket();
}

void discoverStart() {
    if (discoverTaskRunning) {
        return;
    }

    xTaskCreate(
        discoverTask,
        "discover",
        4096,
        NULL,
        1,
        NULL
    );
}

void discoverTask(void *arg) {
    WiFiUDP replyUdp;
    IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();

    replyUdp.begin(DISCOVER_PORT + 1);

    logaMensagem("Enviando cmd discover para o broadcast: %s", broadcast.toString().c_str());
    discoverUdp.beginPacket(broadcast, DISCOVER_PORT);
    discoverUdp.print("{\"cmd\":\"discover\"}");
    discoverUdp.endPacket();
    
    uint32_t inicio = millis();
    logaMensagem("Loop 3s");
    while (millis() - inicio < 3000) {
        int len = replyUdp.parsePacket();
        if (len > 0) {
            logaMensagem("Achei!");

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, replyUdp);
            if (err) {
                Serial.printf("Erro JSON: %s\n", err.c_str());
                continue;
            }

            Serial.printf("Resposta de %s:\n", replyUdp.remoteIP().toString().c_str());
            String s;
            serializeJson(doc, s);
            Serial.write(s.c_str(), s.length());
            Serial.println();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    replyUdp.stop();
    logaMensagem("Scanner completo");

    discoverTaskRunning = false;
    vTaskDelete(NULL);
}
