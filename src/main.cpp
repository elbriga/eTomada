#include <Arduino.h>

#include "recovery.h"
#include "app.h"

static bool modoRecovery = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("====== eTomada ======");
  Serial.println();

  modoRecovery = recoveryBoot();
  if (modoRecovery)
  {
    recoveryInit();
    return;
  }

  appInit();
}

void loop()
{
  if (modoRecovery)
  {
    recoveryLoop();
    delay(5);
    return;
  }
  recoveryBootTick();

  appLoop();

  yield();
}
