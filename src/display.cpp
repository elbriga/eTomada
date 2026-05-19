#include "display.h"
#include "wifi.h"
#include "loga.h"

#ifdef TEM_OLED

#include <SSD1306Wire.h>
#define I2C_DISPLAY_ADDR    0x3C
#define SDA                 5
#define SCL                 4

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

void displayMostraMsg(const char* msg, int timeout, bool loga)
{
    tft.clear();

    tft.drawString(30, 0, "eTomada!");
    tft.drawString(0, 20, msg);

    IPAddress ip = WiFiGetModoAP()
        ? WiFi.softAPIP()
        : WiFi.localIP();

    tft.drawString(0, 40, ip.toString());

    tft.display();

    if (timeout > 0) {
        displayTimeoutMsg = millis() + timeout;
    }

    if (loga) {
        logaMensagem("[DISPLAY][%s]", msg);
    }
}

#else

void displayInit()
{
    logaMensagem("[DISPLAY] OLED desabilitado");
}

bool displayPodeMostrar()
{
    return true;
}

void displayMostraString(int x, int y, const char *msg)
{
    logaMensagem("[DISPLAY] (%d,%d): %s", x, y, msg);
}

void displayMostraMsg(const char* msg, int timeout)
{
    logaMensagem("[DISPLAY] %s", msg);
}

#endif