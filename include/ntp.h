#pragma once

long ntpSyncTime();
void ntpGetTime(struct tm* out, time_t *agora = NULL);
