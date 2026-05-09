#include <Arduino.h>
#include <esp_task_wdt.h>

// NTP
const char* ntpServer1 = "ntp.br";
const char* ntpServer2 = "pool.ntp.org";
// Time zone string
// See list of timezone strings https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* tzInfo = "<-03>3";

void ntpSyncTime() {
  // Start NTP using the two servers above
  configTime(0, 0, ntpServer1, ntpServer2);

  // Set the timezone for your region
  setenv("TZ", tzInfo, 1);
  tzset();

  // Wait until a valid time is received from the NTP server
  // 1577836800 is the Unix time for Jan 1, 2020
  time_t now = 0;
  while (time(&now) < 1577836800) {
    esp_task_wdt_reset(); // alimenta o watchdog
    delay(500);
  }
}

void ntpGetTime(struct tm *out, time_t *agora) {
    time_t now = time(nullptr);
    if (agora != NULL) {
      *agora = now;
    }
    localtime_r(&now, out);
}
