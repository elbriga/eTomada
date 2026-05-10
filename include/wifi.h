#pragma once

#include <WiFi.h>

void WiFiConnect();
void WiFiModoAP();

void WiFiLoop();

String WiFiGetSSID();
bool WiFiTemConfig();
void WiFiSalvaConfig(String ssid, String senha);
void WiFiResetConfig();
