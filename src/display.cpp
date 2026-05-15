#include "display.h"
#include "wifi.h"

// Display OLED
SSD1306Wire tft(I2C_DISPLAY_ADDR, SDA, SCL);

static unsigned long displayTimeoutMsg = 0;

void displayInit()
{
    tft.init();
    tft.clear();
    tft.setFont(ArialMT_Plain_16);
    tft.drawString(30, 0, "eTomada!");
    tft.display();
}

bool displayPodeMostrar()
{
    return displayTimeoutMsg < millis();
}

void displayMostraString(int x, int y, const char *msg)
{
    tft.drawString(x, y, msg);
    tft.display();
}

void displayMostraMsg(const char* msg, int timeout)
{
    tft.clear();
    
    tft.drawString(30, 0, "eTomada!");
    tft.drawString(0, 20, msg);
    
    IPAddress ip = WiFiGetModoAP() ? WiFi.softAPIP() : WiFi.localIP();
    tft.drawString(0, 40, ip.toString());

    tft.display();

    if (timeout > 0) {
        displayTimeoutMsg = millis() + timeout;
    }
}
