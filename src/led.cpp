#include <sys/time.h>

#include "eTomada.h"
#include "loga.h"
#include "led.h"
#include "hardwareProfile.h"
#include "rgb-led.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("LED", nivel, fmt, ##__VA_ARGS__)

extern const HardwareProfile hardwareProfile;

#define TOT_ANIMS 5

typedef struct
{
  bool state;
  bool r, g, b;
  int frame, anim, loop;
} LedT;

static LedT led = {};

void ledInit()
{
  if (!ledAtivo())
    return;

  logaM(LOG_NORMAL, "Ativando led%s de status em [%d]", (hardwareProfile.ledRGB ? " RGB" : ""), hardwareProfile.ledPin);
  pinMode(hardwareProfile.ledPin, OUTPUT);

  if (hardwareProfile.ledRGB)
  {
    rgbLedInit();
    ledSetAnim(1); // Azul == Boot!
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

void ledSetAnim(uint8_t num, uint8_t loop)
{
  if (num >= 0 && num < TOT_ANIMS)
    led.anim = num;
  if (loop > 0 && loop < 11)
    led.loop = loop;
  led.frame = 999; // Forçar mudança
}

void ledProcessa()
{
  if (!ledAtivo())
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
    case RGB_LED_ANIM_BLUE: // 1
      b = led.frame % 2;
      break;
    case RGB_LED_ANIM_RED: // 2
      r = led.frame % 2;
      break;
    case RGB_LED_ANIM_COLOR: // 3
      r = led.frame == 0;
      g = led.frame == 1;
      b = led.frame == 2;
      break;
    case RGB_LED_ANIM_PISCA: // 4
      r = (tv.tv_usec / 50000) % 2;
      g = r;
      b = r;
      break;

    case RGB_LED_ANIM_GREEN: // 0
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
        ledSetAnim(0); // BaseAnim
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
