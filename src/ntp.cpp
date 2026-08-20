#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_sntp.h> // Required for the callback functions

#include "loga.h"
#include "eventos.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("NTP", nivel, fmt, ##__VA_ARGS__)

// NTP
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "a.ntp.br";
// Time zone string
// See list of timezone strings https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char *tzInfo = "<-03>3";

void mainNtpSetSyncFlag();
void ntpTimeSyncCallback(struct timeval *tv);

void ntpSetTZ()
{
  // Set the timezone for your region
  setenv("TZ", tzInfo, 1);
  tzset();
}

void ntpInit()
{
  // Register the callback function
  sntp_set_time_sync_notification_cb(ntpTimeSyncCallback);
}

long ntpSyncTime()
{
  logaM(LOG_NORMAL, "Buscando Data/Hora NTP em background");
  configTime(0, 0, ntpServer1, ntpServer2);

  ntpSetTZ();

  return millis() + 24 * 60 * 60 * 1000; // sync de novo em 24h
}

void ntpTimeSyncCallback(struct timeval *tv)
{
  // callback deve ser rápido!
  mainNtpSetSyncFlag();
}

void sysGetTime(struct tm *out)
{
  time_t now = time(nullptr);
  localtime_r(&now, out);
  if (!out)
    logaM(LOG_CRITICO, "Erro ao buscar hora do sistema!!");
}
