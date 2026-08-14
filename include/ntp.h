#pragma once

void ntpInit();
long ntpSyncTime();
void sysGetTime(struct tm *out);
