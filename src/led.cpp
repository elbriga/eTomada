#include <sys/time.h>

#include "eTomada.h"
#include "loga.h"
#include "led.h"
#include "hardwareProfile.h"
#include "rgb-led.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("LED", nivel, fmt, ##__VA_ARGS__)

extern const HardwareProfile hardwareProfile;

bool ledState;

void ledInit()
{
  if (!ledAtivo())
    return;

  logaM(LOG_NORMAL, "Ativando led%s de status em [%d]", (hardwareProfile.ledRGB ? " RGB" : ""), hardwareProfile.ledPin);
  pinMode(hardwareProfile.ledPin, OUTPUT);

  if (hardwareProfile.ledRGB)
  {
    rgbLedInit();
    rgbLedSetAnim(1); // Azul == Boot!
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

void ledProcessa()
{
  if (!ledAtivo() || hardwareProfile.ledRGB)
    return;

  // Sincronizado com o segundo!
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  bool estado = tv.tv_usec < 100000; // && !((tv.tv_usec % 200000) % 2);

  if (estado != ledState)
  {
    ledState = estado;
    digitalWrite(hardwareProfile.ledPin, hardwareProfile.ledInvertido ? !ledState : ledState);
  }
}
