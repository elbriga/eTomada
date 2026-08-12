#include <Arduino.h>
#include <esp_task_wdt.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("NTP", nivel, fmt, ##__VA_ARGS__)

// NTP
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "a.ntp.br";
// Time zone string
// See list of timezone strings https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char *tzInfo = "<-03>3";

long ntpSyncTime()
{
  // Start NTP using the two servers above
  configTime(0, 0, ntpServer1, ntpServer2);

  // Set the timezone for your region
  setenv("TZ", tzInfo, 1);
  tzset();

  logaM(LOG_NORMAL, "Buscando Data/Hora");

  // Wait until a valid time is received from the NTP server
  // 1577836800 is the Unix time for Jan 1, 2020
  time_t now = 0;
  int count = 0;
  while (time(&now) < 1577836800)
  {
    esp_task_wdt_reset(); // alimenta o watchdog
    delay(500);

    if (count++ > 60)
    {
      logaM(LOG_AVISO, "Falha ao sincronizar hora > Tentar novamente em 1 minuto");
      return millis() + 60 * 1000; // sync de novo em 1 minuto
    }
  }

  return millis() + 24 * 60 * 60 * 1000; // sync de novo em 24h
}

void ntpGetTime(struct tm *out)
{
  time_t now = time(nullptr);
  localtime_r(&now, out);
}
