#pragma once

#include <SSD1306.h>

#define I2C_DISPLAY_ADDR    0x3C
#define SDA                 5
#define SCL                 4

extern SSD1306Wire tft;

void displayInit();
bool displayPodeMostrar();
void displayMostraMsg(const char* msg, int timeout = 0);
void displayMostraString(int x, int y, const char *msg);
