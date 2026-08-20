#pragma once

void ntpInit();
void ntpSetTZ();
long ntpSyncTime();
void sysGetTime(struct tm *out);
