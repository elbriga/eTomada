#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <DNSServer.h>

#include "eTomada.h"
#include "display.h"
#include "wifi.h"
#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("WIFI", nivel, fmt, ##__VA_ARGS__)

static Preferences wifiPrefs;
static DNSServer dnsServer;

// Timeout
#define MODO_AP_MAX_TEMPO_IDLE 60000
static long tempoIdleModoAP = 0;

// Scanning
static bool wifiScanning = false;
static unsigned long lastWiFiScan = 0;
static JsonDocument wifiScanDoc;

void WiFiConnect()
{
  // Para testes
  // WiFiSalvaConfig("GLS", "09876543");

  wifiPrefs.begin("wifi", true);
  String ssid = wifiPrefs.getString("ssid", "");
  String pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();

  if (ssid == "")
  {
    logaM(LOG_AVISO, "Sem WiFi configurado");
    WiFiModoAP();
    return;
  }

  // Connect to Wifi.
  logaM(LOG_NORMAL, "Conectando a rede [%s]", ssid.c_str());

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);

  String hostname = "eTomada-" + eTomadaDeviceID();
  WiFi.setHostname(hostname.c_str());

  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    esp_task_wdt_reset(); // alimenta o watchdog

    if (WiFi.status() == WL_CONNECT_FAILED)
    {
      logaM(LOG_AVISO, "Falha!! Cheque a configuracao!!");
      delay(5000);
    }

    if (millis() - start > 20000)
    {
      logaM(LOG_AVISO, "Timeout WiFi");

      WiFi.disconnect(true);

      WiFiModoAP();
      return;
    }

    Serial.print(".");
    delay(500);
  }

  Serial.println();
  WiFi.setSleep(false);

  logaM(LOG_NORMAL, "Endereço IP: [%s]@[%s] em [%s]",
        WiFi.getHostname(), WiFi.localIP().toString().c_str(), ssid.c_str());
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
  tempoIdleModoAP = millis();

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
  if (!wifiScanning)
  {
    if (millis() - lastWiFiScan > 30000)
    {
      WiFiStartScan();
    }
    return;
  }

  int numRedes = WiFi.scanComplete();
  if (numRedes == -1)
  {
    // ainda escaneando
    return;
  }
  if (numRedes < 0)
  {
    wifiScanning = false;
    wifiScanDoc.clear();
    wifiScanDoc["scanning"] = false;
    wifiScanDoc["coderro"] = numRedes;
    logaM(LOG_AVISO, "Erro scan WiFi [%d]", numRedes);
    return;
  }

  wifiScanDoc.clear();
  wifiScanDoc["scanning"] = false;

  JsonArray arr = wifiScanDoc["redes"].to<JsonArray>();
  for (int i = 0; i < numRedes; i++)
  {
    String ssid = WiFi.SSID(i);
    if (!ssid.length())
    {
      continue;
    }

    JsonObject r = arr.add<JsonObject>();
    r["ssid"] = ssid;
    r["rssi"] = WiFi.RSSI(i);
    r["enc"] = WiFi.encryptionType(i);
  }

  WiFi.scanDelete();

  logaM(LOG_DEBUG0, "Scan OK [%d redes]", numRedes);

  wifiScanning = false;
}

void WiFiModoAPLoop()
{
  dnsServer.processNextRequest();

  if (millis() - tempoIdleModoAP > MODO_AP_MAX_TEMPO_IDLE)
  {
    // Resetar para caso seja uma falha temporária no wifi
    logaTitulo("RESET!");
    ESP.restart();
  }

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
  logaM(LOG_AVISO, "=== MODO AP ===");
  logaM(LOG_AVISO, "IP: [%s]", ip.toString().c_str());

  WiFiStartScan();

  displayMostraMsg("Configure o WiFi!");

  tempoIdleModoAP = millis();
}

void WiFiStartScan()
{
  if (wifiScanning)
  {
    return;
  }

  wifiScanDoc.clear();
  wifiScanDoc["scanning"] = true;
  wifiScanDoc["redes"].to<JsonArray>();

  WiFi.scanDelete();
  WiFi.scanNetworks(true);

  logaM(LOG_DEBUG0, "WiFi scan iniciado");

  lastWiFiScan = millis();
  wifiScanning = true;
}

String WiFiGetScanJSON()
{
  tempoIdleModoAP = millis();
  String out;
  serializeJson(wifiScanDoc, out);
  return out;
}
