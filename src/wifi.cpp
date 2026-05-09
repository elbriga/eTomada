#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

// WiFi credentials.
const char* WIFI_SSID = "Ligga Gabriel 2.4g";
const char* WIFI_PASS = "09876543";

void WiFiConnect() {
  // Connect to Wifi.
  Serial.print("Conectando a rede [");
  Serial.print(WIFI_SSID);
  Serial.print("]: ");

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    esp_task_wdt_reset(); // alimenta o watchdog

    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Falha!! Cheque a configuracao!!");
      Serial.println();
      delay(5000);
    }
    delay(500);
  }
  Serial.println("OK!");

  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP().toString());
}
