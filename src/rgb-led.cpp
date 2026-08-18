#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "rgb-led.h"
#include "loga.h"
#include "hardwareProfile.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RGBLED", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

#define INTENSIDADE 0.1
#define MAX_FRAMES 4
#define TOT_ANIMS 4

typedef struct
{
    uint8_t r, g, b;
} rgbLedFrame;

typedef struct
{
    uint8_t totalFrames;
    rgbLedFrame frames[MAX_FRAMES];
} rgbLedAnim;

rgbLedAnim animacoes[TOT_ANIMS] = {
    {.totalFrames = 4, .frames = {
                           {0, 255, 0},
                           {0, 0, 0},
                           {0, 0, 0},
                           {0, 0, 0},
                       }},
    {.totalFrames = 4, .frames = {
                           {0, 0, 255},
                           {0, 0, 0},
                           {0, 0, 255},
                           {0, 0, 0},
                       }},
    {.totalFrames = 4, .frames = {
                           {255, 0, 0},
                           {0, 0, 0},
                           {255, 0, 0},
                           {0, 0, 0},
                       }},
    {.totalFrames = 4, .frames = {
                           {255, 0, 0},
                           {0, 255, 0},
                           {0, 0, 255},
                           {0, 0, 0},
                       }},
};

static Adafruit_NeoPixel led;

void rgbLedProcessaTask(void *);
void rgbLedInit()
{
    led = Adafruit_NeoPixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

    led.begin();
    led.clear();
    led.show();

    xTaskCreatePinnedToCore(
        rgbLedProcessaTask,
        "rgbLed",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b)
{
    led.setPixelColor(0, r * INTENSIDADE, g * INTENSIDADE, b * INTENSIDADE);
    led.show();
}

void rgbLedOff()
{
    led.clear();
    led.show();
}

static uint8_t Anim = 0;
static uint8_t BaseAnim = 0;
static uint8_t Frame = 0;
static uint8_t ZeroCount = 0;
void rgbLedProcessaTask(void *)
{
    while (true)
    {
        rgbLedWrite(animacoes[Anim].frames[Frame].r,
                    animacoes[Anim].frames[Frame].g,
                    animacoes[Anim].frames[Frame].b);

        Frame++;
        if (Frame >= animacoes[Anim].totalFrames || Frame >= MAX_FRAMES)
        {
            Frame = 0;
            if (ZeroCount)
            {
                ZeroCount--;
                if (!ZeroCount)
                    rgbLedSetAnim(BaseAnim);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void rgbLedSetAnim(uint8_t num, uint8_t loop)
{
    if (num >= 0 && num < TOT_ANIMS)
        Anim = num;
    if (loop > 0 && loop < 11)
        ZeroCount = loop;
}

void rgbLedSetBaseAnim(uint8_t num)
{
    if (num >= 0 && num < TOT_ANIMS)
        BaseAnim = num;
}
