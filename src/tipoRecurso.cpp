#include <Arduino.h>

#include "tipoRecurso.h"

ComandoRecurso comandoRecursoGetFromString(String comando)
{
  ComandoRecurso ret = COMANDO_NENHUM;

  if (comando == "ON")
    ret = COMANDO_ON;
  else if (comando == "OFF")
    ret = COMANDO_OFF;
  else if (comando == "PULSE")
    ret = COMANDO_PULSE;
  else if (comando == "TOGGLE")
    ret = COMANDO_TOGGLE;

  return ret;
}

const char *comandoRecursoGetString(ComandoRecurso comando)
{
  switch (comando)
  {
  case COMANDO_ON:
    return "ON";
  case COMANDO_OFF:
    return "OFF";
  case COMANDO_TOGGLE:
    return "TOGGLE";
  case COMANDO_PULSE:
    return "PULSE";
  default:
    return "NENHUM";
  }
}
