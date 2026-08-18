#pragma once
#include <stdint.h>

#define RGB_LED_PIN 48

#define RGB_LED_ANIM_GREEN 0

void rgbLedInit();
void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b);
void rgbLedOff();

void rgbLedSetAnim(uint8_t num, uint8_t loop = 0);
