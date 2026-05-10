#include "display.h"
#include "wifi.h"

static unsigned long displayTimeoutMsg = 0;

void displayInit() {
    tft.init();
    tft.clear();
    tft.setFont(ArialMT_Plain_16);
    tft.drawString(30, 0, "eTomada!");
    tft.display();
}

bool displayPodeMostrar() {
    return displayTimeoutMsg < millis();
}

void displayMostraMsg(const char* msg, int timeout) {
    IPAddress ip = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP();

    tft.clear();
    tft.drawString(30, 0, "eTomada!");
    tft.drawString(0, 20, msg);
    tft.drawString(0, 40, ip.toString());
    tft.display();

    if (timeout > 0)
        displayTimeoutMsg = millis() + timeout;
}
