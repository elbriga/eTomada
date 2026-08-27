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

  logaM(LOG_NORMAL, "Ativando led de status em [%d]", hardwareProfile.ledPin);
  pinMode(hardwareProfile.ledPin, OUTPUT);                             // Led Azul do MINI
  digitalWrite(hardwareProfile.ledPin, !hardwareProfile.ledInvertido); // No MINI LOW = Aceso!

  if (hardwareProfile.ledPin == RGB_LED_PIN)
  {
    rgbLedInit();
    rgbLedSetAnim(1); // Azul == Boot!
  }
}

bool ledAtivo()
{
  return hardwareProfile.ledPin != 255;
}

void ledProcessa()
{
  if (!ledAtivo())
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
