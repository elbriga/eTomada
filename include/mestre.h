#pragma once
#include <Arduino.h>

void mestreInit();
bool mestreAtivo();

IPAddress mestreGetIP();
const char *mestreGetID();

void mestreEnviaEvento(String payloadJson);
