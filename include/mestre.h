#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct Mestre
{
    String deviceID;
    IPAddress ip;
};

void mestreInit();
void mestreLoop();
bool mestreAtivo();

IPAddress mestreGetIP();
const char *mestreGetID();

void mestreEnviaEvento(String payloadJson);
