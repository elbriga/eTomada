#pragma once
#include <ArduinoJson.h>

struct Recurso; // Forward declaration

enum LedAnim
{
    LED_ANIM_OFF = 0,
    LED_ANIM_ALARM = 1,
    LED_ANIM_RUN = 2,
    LED_ANIM_BOOT = 3,
    LED_ANIM_COLOR = 4,
    LED_ANIM_PISCA = 5,
};

struct LedRGB
{
    LedAnim anim;
};

void ledInit();
bool ledAtivo();
bool ledEhRGB();
LedRGB *ledGet();

bool ledSetAnim(LedAnim num, uint8_t loop = 0, String *msgOut = nullptr);
LedAnim ledGetAnim();
JsonDocument ledGetJSONDoc(Recurso *r, bool full = true);

void ledIdle();
void ledAlarm();

void ledProcessa();
