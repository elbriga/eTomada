#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <DNSServer.h>

#include "display.h"
#include "wifi.h"

static Preferences wifiPrefs;
static DNSServer dnsServer;

void WiFiConnect()
{
  wifiPrefs.begin("wifi", true);
  String ssid = wifiPrefs.getString("ssid", "");
  String pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();

  if (ssid == "") {
    Serial.println("Sem WiFi configurado");
    WiFiModoAP();
    return;
  }

  // Connect to Wifi.
  Serial.print("Conectando a rede [" + ssid + "]");

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
      Serial.println("Falha!! Cheque a configuracao!!");
      Serial.println();
      delay(5000);
    }

    if (millis() - start > 20000) {
      Serial.println("Timeout WiFi");

      WiFi.disconnect(true);

      WiFiModoAP();
      return;
    }

    delay(500);
  }
  Serial.println("OK!");

  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP().toString());
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

void WiFiLoop()
{
  dnsServer.processNextRequest();
}

void WiFiModoAP()
{
  WiFi.mode(WIFI_AP);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  
  uint64_t chipid = ESP.getEfuseMac();
  char nomeAP[32];
  sprintf(nomeAP, "eTomadaSetup-%04X", (uint16_t)(chipid & 0xFFFF));
  WiFi.softAP(nomeAP, "09876543");

  dnsServer.start(53, "*", apIP);

  IPAddress ip = WiFi.softAPIP();
  Serial.println("");
  Serial.println("=== MODO AP ===");
  Serial.print("IP: ");
  Serial.println(ip);

  displayMostraMsg("Modo AP");
}
