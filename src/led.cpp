#include <sys/time.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "eTomada.h"
#include "loga.h"
#include "led.h"
#include "hardwareProfile.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("LED", nivel, fmt, ##__VA_ARGS__)

extern const HardwareProfile hardwareProfile;

#define TOT_ANIMS 5
#define INTENSIDADE 0.1

typedef struct
{
  bool state;
  bool r, g, b;
  int frame, anim, loop;
} LedT;

static LedT led = {};
static Adafruit_NeoPixel rgbLed;

void ledInit()
{
  if (!ledAtivo())
    return;

  logaM(LOG_NORMAL, "Ativando led%s de status em [%d]", (hardwareProfile.ledRGB ? " RGB" : ""), hardwareProfile.ledPin);
  pinMode(hardwareProfile.ledPin, OUTPUT);

  if (hardwareProfile.ledRGB)
  {
    rgbLed = Adafruit_NeoPixel(1, hardwareProfile.ledPin, NEO_GRB + NEO_KHZ800);

    rgbLed.begin();
    rgbLed.clear();
    rgbLed.show();

    ledSetAnim(LED_ANIM_BOOT); // Azul == Boot!
  }
  else
  {
    digitalWrite(hardwareProfile.ledPin, !hardwareProfile.ledInvertido);
  }
}

bool ledAtivo()
{
  return hardwareProfile.ledPin != 255;
}

int ledGetAnim()
{
  return led.anim;
}

void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b)
{
  if (!r && !g && !b)
    rgbLed.clear();
  else
    rgbLed.setPixelColor(0, r * INTENSIDADE, g * INTENSIDADE, b * INTENSIDADE);
  rgbLed.show();
}

void ledSetAnim(uint8_t num, uint8_t loop)
{
  if (num == LED_ANIM_OFF || (num >= 0 && num < TOT_ANIMS))
    led.anim = num;
  if (loop > 0 && loop < 11)
    led.loop = loop;
  led.frame = 999; // Forçar mudança

  if (num == LED_ANIM_OFF)
    rgbLedWrite(0, 0, 0);
}

void ledProcessa()
{
  if (!ledAtivo())
    return;

  if (led.anim == LED_ANIM_OFF)
    return;

  // Sincronizado com o segundo!
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  if (hardwareProfile.ledRGB)
  {
    int oldFrame = led.frame;
    led.frame = (tv.tv_usec / 250000);

    bool r = false, g = false, b = false;
    switch (led.anim)
    {
    case LED_ANIM_BOOT: // 1
      b = led.frame % 2;
      break;
    case LED_ANIM_ALARM: // 2
      r = led.frame % 2;
      break;
    case LED_ANIM_COLOR: // 3
      r = led.frame == 0;
      g = led.frame == 1;
      b = led.frame == 2;
      break;
    case LED_ANIM_PISCA: // 4
      r = (tv.tv_usec / 50000) % 2;
      g = r;
      b = r;
      break;

    case LED_ANIM_RUN: // 0
    default:
      g = led.frame == 0;
      break;
    }

    if (led.r != r || led.g != g || led.b != b)
    {
      led.r = r;
      led.g = g;
      led.b = b;
      rgbLedWrite(r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
    }

    if (led.frame == 0 && oldFrame > 0 && led.loop > 0)
    {
      led.loop--;
      if (!led.loop && led.anim != LED_ANIM_ALARM) // eTomadaEmAlerta()!
        ledSetAnim(0);                             // BaseAnim
    }
  }
  else
  {
    // TODO :: variar conforme Anim
    bool estado = tv.tv_usec < 100000; // && !((tv.tv_usec % 200000) % 2);

    if (estado != led.state)
    {
      led.state = estado;
      digitalWrite(hardwareProfile.ledPin, hardwareProfile.ledInvertido ? !led.state : led.state);
    }
  }
}
