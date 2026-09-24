#pragma once

#define RGB_LED_ANIM_GREEN 0
#define RGB_LED_ANIM_BLUE 1
#define RGB_LED_ANIM_RED 2
#define RGB_LED_ANIM_COLOR 3
#define RGB_LED_ANIM_PISCA 4

void ledInit();
bool ledAtivo();
void ledSetAnim(uint8_t num, uint8_t loop = 0);
void ledProcessa();
