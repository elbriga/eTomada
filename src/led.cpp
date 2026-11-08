#include <sys/time.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "eTomada.h"
#include "loga.h"
#include "led.h"
#include "hardwareProfile.h"
#include "regras.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("LED", nivel, fmt, ##__VA_ARGS__)

extern const HardwareProfile hardwareProfile;

#define TOT_ANIMS 5
#define INTENSIDADE 0.33

typedef struct
{
  bool state;
  bool r, g, b;
  int frame, anim, loop;
} LedT;

static LedT led = {};
static Adafruit_NeoPixel rgbLed;

// estado exportado para fora
static LedRGB ledRGB;

void ledInit()
{
  if (!ledAtivo())
    return;

  logaM(LOG_NORMAL, "Ativando led%s de status em [%d]", (hardwareProfile.led.rgb ? " RGB" : ""), hardwareProfile.led.pino);
  pinMode(hardwareProfile.led.pino, OUTPUT);

  if (hardwareProfile.led.rgb)
  {
    rgbLed = Adafruit_NeoPixel(1, hardwareProfile.led.pino, NEO_GRB + NEO_KHZ800);

    rgbLed.begin();
    rgbLed.clear();
    rgbLed.show();
  }
  else
  {
    digitalWrite(hardwareProfile.led.pino, !hardwareProfile.led.invertido);
  }

  ledSetAnim(LED_ANIM_BOOT); // Azul == Boot!
}

bool ledAtivo()
{
  return hardwareProfile.led.pino != 255;
}

bool ledEhRGB()
{
  return hardwareProfile.led.rgb;
}

LedRGB *ledGet()
{
  return &ledRGB;
}

LedAnim ledGetAnim()
{
  return ledRGB.anim;
}

void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b)
{
  if (!r && !g && !b)
    rgbLed.clear();
  else
    rgbLed.setPixelColor(0, r * INTENSIDADE, g * INTENSIDADE, b * INTENSIDADE);
  rgbLed.show();
}

bool ledSetAnim(LedAnim num, uint8_t loop, String *msgOut)
{
  if (num < 0 || num >= TOT_ANIMS)
  {
    if (msgOut)
      *msgOut = "ledSetAnim :: Anim invalida!";
    return false;
  }

  led.anim = num;
  ledRGB.anim = (LedAnim)led.anim;

  if (loop > 0 && loop < 11)
    led.loop = loop;
  led.frame = 999; // Forçar mudança

  if (num == LED_ANIM_OFF)
    rgbLedWrite(0, 0, 0);

  if (msgOut)
    *msgOut = "OK";
  return true;
}

void ledIdle()
{
  bool pisca = false;
  ledSetAnim(pisca ? LED_ANIM_RUN : LED_ANIM_OFF);
}

void ledAlarm()
{
  ledSetAnim(LED_ANIM_ALARM);
}

JsonDocument ledGetJSONDoc(Recurso *r, bool full)
{
  JsonDocument doc;

  LedRGB *l = r->led;
  if (!l)
    return doc;

  doc["anim"] = l->anim;

  return doc;
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

  if (hardwareProfile.led.rgb)
  {
    int oldFrame = led.frame;
    led.frame = (tv.tv_usec / 250000);

    bool r = false, g = false, b = false;
    switch (led.anim)
    {
    case LED_ANIM_BOOT:
      b = led.frame % 2;
      break;
    case LED_ANIM_ALARM:
      r = led.frame % 2;
      break;
    case LED_ANIM_COLOR:
      r = led.frame == 0;
      g = led.frame == 1;
      b = led.frame == 2;
      break;
    case LED_ANIM_PISCA:
      r = (tv.tv_usec / 50000) % 2;
      g = r;
      b = r;
      break;

    case LED_ANIM_RUN:
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
      if (!led.loop)
        ledIdle();
    }
  }
  else
  {
    // TODO :: variar conforme Anim
    bool estado = tv.tv_usec < 100000; // && !((tv.tv_usec % 200000) % 2);

    if (estado != led.state)
    {
      led.state = estado;
      digitalWrite(hardwareProfile.led.pino, hardwareProfile.led.invertido ? !led.state : led.state);
    }
  }
}
