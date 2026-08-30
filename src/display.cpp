#include "eTomada.h"
#include "display.h"
#include "wifi.h"
#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("DISPLAY", nivel, fmt, ##__VA_ARGS__)

#ifdef TELA_COLORIDA

#include <TFT_eSPI.h>
#include "images/background.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite imgBuffer = TFT_eSprite(&tft); // Double buffer

static unsigned long displayTimeoutMsg = 0;

void displayInit()
{
    tft.init();
    tft.setRotation(3); // Landscape orientation
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); // Turn on backlight

    tft.pushImage(0, 0, 240, 135, BackgroundTelaColorida);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(145, 33, 2);
    tft.println("eTomada!");

    // Allocate RAM block for the canvas buffer (240 x 135 pixels)
    if (imgBuffer.createSprite(240, 135) == nullptr)
        logaM(LOG_CRITICO, "Sem RAM para double buffer!");
    imgBuffer.setSwapBytes(true);
    imgBuffer.setTextColor(TFT_GREEN, TFT_BLACK);
}

bool displayPodeMostrar()
{
    return (long)(millis() - displayTimeoutMsg) >= 0;
}

void displayMostraString(int x, int y, const char *msg)
{
    tft.setCursor(x, y, 2);
    tft.printf("%s", msg);
}

void displayMostraMsg(const char *msg, int timeout, bool logar)
{
    imgBuffer.pushImage(0, 0, 240, 135, BackgroundTelaColorida);

    imgBuffer.setCursor(147, 33, 2);
    imgBuffer.printf("%s", eTomadaDeviceID().c_str());

    IPAddress ip = WiFiGetModoAP() ? WiFi.softAPIP() : WiFi.localIP();
    imgBuffer.setCursor(147, 61, 2);
    imgBuffer.printf("%s", ip.toString().c_str());

    imgBuffer.setCursor(147, 87, 2);
    imgBuffer.printf("%s", msg);

    imgBuffer.pushSprite(0, 0);

    if (timeout > 0)
        displayTimeoutMsg = millis() + timeout;

    if (logar)
        logaM(LOG_NORMAL, "DISPLAY [%s]", msg);
}

#else

void displayInit()
{
    logaM(LOG_NORMAL, "DISPLAY desabilitado");
}

bool displayPodeMostrar()
{
    return true;
}

void displayMostraString(int x, int y, const char *msg)
{
    logaM(LOG_NORMAL, "(%d,%d): %s", x, y, msg);
}

void displayMostraMsg(const char *msg, int timeout, bool logar)
{
    if (logar)
        logaM(LOG_NORMAL, "> %s", msg);
}

#endif