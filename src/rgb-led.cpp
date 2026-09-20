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
    led = Adafruit_NeoPixel(1, hardwareProfile.ledPin, NEO_GRB + NEO_KHZ800);

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
static uint8_t Frame = 100;
static uint8_t ZeroCount = 0;
void rgbLedProcessaTask(void *)
{
    while (true)
    {
        // Sincronizado com o segundo!
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        int ms = (tv.tv_usec / 1000);

        int newFrame = (ms * animacoes[Anim].totalFrames) / 1000;

        if (newFrame == Frame)
        {
            vTaskDelay(pdMS_TO_TICKS(25));
            continue;
        }

        int oldFrame = Frame;
        Frame = newFrame;

        rgbLedWrite(animacoes[Anim].frames[Frame].r,
                    animacoes[Anim].frames[Frame].g,
                    animacoes[Anim].frames[Frame].b);

        if (!Frame && oldFrame && ZeroCount)
        {
            ZeroCount--;
            if (!ZeroCount)
                rgbLedSetAnim(BaseAnim);
        }
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
    rgbLedSetAnim(num);
}
