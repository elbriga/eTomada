#pragma once

#include <WiFi.h>

void WiFiConnect();
void WiFiModoAP();
bool WiFiGetModoAP();

void WiFiLoop();

String WiFiGetSSID();
bool WiFiTemConfig();
void WiFiSalvaConfig(String ssid, String senha);
void WiFiResetConfig();

void WiFiStartScan();
String WiFiGetScanJSON();
