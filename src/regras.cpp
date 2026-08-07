#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "display.h"
#include "mutex.h"

void processaRegras()
{
  if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
  {
    return;
  }

  String msgDisplay = "";
  {
    MutexLock lockRecursos(recursosMutex);
    if (!lockRecursos)
    {
      logaMensagem("processaRegras: erro mutex");
      return;
    }
  }

  if (msgDisplay != "")
  {
    displayMostraMsg(msgDisplay.c_str(), 5000, false);
  }
}

String validaRegra(String regra)
{
  if (regra == "")
  {
    return "OK";
  }

  return "TODO";
}
