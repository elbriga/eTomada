#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <DNSServer.h>

#include "display.h"
#include "wifi.h"
#include "loga.h"

static Preferences wifiPrefs;
static DNSServer dnsServer;

// Scanning
static bool wifiScanning = false;
static unsigned long lastWiFiScan = 0;
static JsonDocument wifiScanDoc;

void WiFiConnect()
{
  wifiPrefs.begin("wifi", true);
  String ssid = wifiPrefs.getString("ssid", "");
  String pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();

  if (ssid == "") {
    logaMensagem("Sem WiFi configurado");
    WiFiModoAP();
    return;
  }

  // Connect to Wifi.
  logaMensagem("Conectando a rede [%s]", ssid.c_str());

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);

  delay(100);

  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    esp_task_wdt_reset(); // alimenta o watchdog

    if (WiFi.status() == WL_CONNECT_FAILED) {
      logaMensagem("Falha!! Cheque a configuracao!!");
      delay(5000);
    }

    if (millis() - start > 20000) {
      logaMensagem("Timeout WiFi");

      WiFi.disconnect(true);

      WiFiModoAP();
      return;
    }

    delay(500);
  }

  logaMensagem("Endereço IP: [%s]", WiFi.localIP().toString().c_str());
}

bool WiFiGetModoAP()
{
  wifi_mode_t mode = WiFi.getMode();
  return mode == WIFI_AP_STA || mode == WIFI_AP;
}

String WiFiGetSSID()
{
  wifiPrefs.begin("wifi", true);

  String ssid = wifiPrefs.getString("ssid", "");

  wifiPrefs.end();

  return ssid;
}

void WiFiSalvaConfig(String ssid, String senha)
{
  wifiPrefs.begin("wifi", false);

  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", senha);

  wifiPrefs.end();
}

void WiFiResetConfig()
{
  WiFiSalvaConfig("", "");
}

bool WiFiTemConfig()
{
  return WiFiGetSSID() != "";
}

void WiFiScanLoop()
{
  if (!wifiScanning) {
    if (millis() - lastWiFiScan > 30000) {
      WiFiStartScan();
    }
    return;
  }

  int numRedes = WiFi.scanComplete();
  if (numRedes == -1) {
    // ainda escaneando
    return;
  }
  if (numRedes < 0) {
    wifiScanning = false;
    wifiScanDoc.clear();
    wifiScanDoc["scanning"] = false;
    wifiScanDoc["coderro"]  = numRedes;
    logaMensagem("Erro scan WiFi [%d]", numRedes);
    return;
  }

  wifiScanDoc.clear();
  wifiScanDoc["scanning"] = false;

  JsonArray arr = wifiScanDoc["redes"].to<JsonArray>();
  for (int i = 0; i < numRedes; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) {
      continue;
    }

    JsonObject r = arr.add<JsonObject>();
    r["ssid"] = ssid;
    r["rssi"] = WiFi.RSSI(i);
    r["enc"] = WiFi.encryptionType(i);
  }

  WiFi.scanDelete();
  
  //logaMensagem("Scan OK [%d redes]", numRedes);
  wifiScanning = false;
}

void WiFiLoop()
{
  dnsServer.processNextRequest();

  WiFiScanLoop();
}

void WiFiModoAP()
{
  WiFi.mode(WIFI_AP_STA);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  
  uint64_t chipid = ESP.getEfuseMac();
  char nomeAP[32];
  sprintf(nomeAP, "eTomadaSetup-%04X", (uint16_t)(chipid & 0xFFFF));
  WiFi.softAP(nomeAP, "09876543");

  dnsServer.start(53, "*", apIP);

  IPAddress ip = WiFi.softAPIP();
  logaMensagem("=== MODO AP ===");
  logaMensagem("IP: [%s]", ip.toString().c_str());

  WiFiStartScan();

  displayMostraMsg("Configure o WiFi!");
}

void WiFiStartScan()
{
  if (wifiScanning) {
    return;
  }

  wifiScanDoc.clear();
  wifiScanDoc["scanning"] = true;
  wifiScanDoc["redes"].to<JsonArray>();

  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  
  //logaMensagem("WiFi scan iniciado");

  lastWiFiScan = millis();
  wifiScanning = true;
}

String WiFiGetScanJSON() {
  String out;
  serializeJson(wifiScanDoc, out);
  return out;
}
