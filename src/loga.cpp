#include <Arduino.h>
#include <stdarg.h>

#include "loga.h"

void logaMensagem(const char* fmt, ...) {
  char msg[512];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Obter horario
  time_t agora = time(nullptr);
  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);
  char formattedTime[32] = {0};
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);

  Serial.printf("[%s] %s\n", formattedTime, msg);
}
