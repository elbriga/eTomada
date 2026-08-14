#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RTC", nivel, fmt, ##__VA_ARGS__)

RTC_DS3231 rtc;
bool rtcOK = false;

void rtcSetSystemClock();

void rtcInit()
{
    rtcOK = rtc.begin();

    if (!rtcOK)
    {
        logaM(LOG_AVISO, "RTC não encontrado!");
        return;
    }

    DateTime now = rtc.now();

    logaM(LOG_NORMAL, "RTC inicializado [%02d/%02d/%04d %02d:%02d:%02d]",
          now.day(), now.month(), now.year(),
          now.hour(), now.minute(), now.second());

    // Validar a hora do RTC
    if (now.year() < 2026)
    {
        logaM(LOG_AVISO, "Data/Hora do RTC invalida [%d] - descartando e usando NTP", now.year());
        return;
    }

    rtcSetSystemClock();
}

bool rtcAtivo()
{
    return rtcOK;
}

void rtcSetSystemClock()
{
    DateTime now = rtc.now();

    struct tm tm_time;
    tm_time.tm_year = now.year() - 1900; // tm_year counts years since 1900
    tm_time.tm_mon = now.month() - 1;    // tm_mon is 0-11 (Jan-Dec)
    tm_time.tm_mday = now.day();
    tm_time.tm_hour = now.hour();
    tm_time.tm_min = now.minute();
    tm_time.tm_sec = now.second();
    tm_time.tm_isdst = -1;

    time_t epoch_seconds = mktime(&tm_time);

    // Define timeval structural values for the OS kernel
    struct timeval tv = {.tv_sec = epoch_seconds, .tv_usec = 0};

    // Overwrite the ESP32 system clock instantly
    settimeofday(&tv, NULL);

    logaM(LOG_NORMAL, "Data/Hora atualizada com RTC");
}
