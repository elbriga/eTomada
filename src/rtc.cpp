#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

#include "loga.h"
#include "ntp.h"

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

// Pega o relogio do RTC e seta no relogio do sistema
void rtcSetSystemClock()
{
    if (!rtcAtivo())
        return;

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

// Pega o relogio atual do sistema e grava no RTC
void rtcStoreSystemClock()
{
    if (!rtcAtivo())
        return;

    // Obter a hora
    struct tm timeinfo;
    sysGetTime(&timeinfo);

    // Convert 'tm' structure to RTClib 'DateTime' structure
    // tm_year starts from 1900, tm_mon is 0-11
    DateTime ntpTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    rtc.adjust(ntpTime);

    logaM(LOG_AVISO, "A hora do RTC DS3231 foi atualizada");
}

// Force-resets the internal ESP32 system clock back to 1970
void rtcForceResetSystemTime()
{
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    settimeofday(&tv, NULL);
    logaM(LOG_AVISO, "System time has been force-reset to 01/01/1970");
}
