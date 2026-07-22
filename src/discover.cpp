#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "eTomada.h"
#include "loga.h"

#define DISCOVER_PORT 8266

static WiFiUDP discoverUdp;
void discoverTask(void *arg);

void discoverInit() {
    discoverUdp.begin(DISCOVER_PORT);
}

void discoverLoop() {
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
    discoverUdp.print(eTomadaGetSnapshotJSON());
    discoverUdp.endPacket();
}

void discoverStart() {
    xTaskCreate(
        discoverTask,
        "discover",
        4096,
        NULL,
        1,
        NULL
    );
}

// TODO :: O GLOBAL discoverUdp NAO É THREAD SAFE!!
// TODO :: O GLOBAL discoverUdp NAO É THREAD SAFE!!
// TODO :: O GLOBAL discoverUdp NAO É THREAD SAFE!!
void discoverTask(void *arg) {
    WiFiUDP replyUdp;
    IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();

    replyUdp.begin(DISCOVER_PORT + 1);

    Serial.printf("Enviando cmd discover para o broadcast: %s\n", broadcast.toString().c_str());
    discoverUdp.beginPacket(broadcast, DISCOVER_PORT);
    discoverUdp.print("{\"cmd\":\"discover\"}");
    discoverUdp.endPacket();
    
    uint32_t inicio = millis();
    Serial.println("Loop 3s");
    while (millis() - inicio < 3000) {
        int len = replyUdp.parsePacket();
        if (len > 0) {
            Serial.println("Achei!");

            char buf[2048];
            len = replyUdp.read(buf, sizeof(buf) - 1);
            buf[len] = 0;
            
            Serial.printf(
                "Resposta %s : %s\n",
                replyUdp.remoteIP().toString().c_str(),
                buf
            );
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    replyUdp.stop();
    Serial.println("Done!");

    vTaskDelete(NULL);
}
