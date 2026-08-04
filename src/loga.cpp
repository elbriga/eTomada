#include <Arduino.h>
#include <stdarg.h>

#include "loga.h"
#include "ntp.h"

void logaMensagem(const char *fmt, ...)
{
  char msg[512];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Obter horario
  struct tm timeinfo;
  ntpGetTime(&timeinfo);

  char formattedTime[32] = {0};
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);

  char formattedUptime[32] = {0};
  int uptime = millis() / 1000;
  int dias = uptime / 86400;
  int horas = (uptime % 86400) / 3600;
  int minutos = (uptime % 3600) / 60;
  int segundos = uptime % 60;
  if (dias > 0)
  {
    snprintf(formattedUptime, sizeof(formattedUptime), "%dd %s%d:%s%d:%s%d",
             dias,
             (horas < 10 ? "0" : ""), horas,
             (minutos < 10 ? "0" : ""), minutos,
             (segundos < 10 ? "0" : ""), segundos);
  }
  else
  {
    snprintf(formattedUptime, sizeof(formattedUptime), "%s%d:%s%d:%s%d",
             (horas < 10 ? "0" : ""), horas,
             (minutos < 10 ? "0" : ""), minutos,
             (segundos < 10 ? "0" : ""), segundos);
  }

  Serial.printf("[%s][%s] %s\n", formattedTime, formattedUptime, msg);
}

void logaTitulo(const char *msg)
{
  logaMensagem("====");
  logaMensagem("== %s ==", msg);
  logaMensagem("====");
}
