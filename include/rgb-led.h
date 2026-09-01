#pragma once
#include <stdint.h>

#define RGB_LED_PIN 48

#define RGB_LED_ANIM_GREEN 0
#define RGB_LED_ANIM_BLUE 1
#define RGB_LED_ANIM_RED 2
#define RGB_LED_ANIM_COLOR 3

void rgbLedInit();
void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b);
void rgbLedOff();

void rgbLedSetAnim(uint8_t num, uint8_t loop = 0);
void rgbLedSetBaseAnim(uint8_t num);
