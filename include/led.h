#pragma once

#define LED_ANIM_OFF 99
#define LED_ANIM_RUN 0
#define LED_ANIM_BOOT 1
#define LED_ANIM_ALARM 2
#define LED_ANIM_COLOR 3
#define LED_ANIM_PISCA 4

void ledInit();
bool ledAtivo();
void ledSetAnim(uint8_t num, uint8_t loop = 0);
int ledGetAnim();
void ledProcessa();
