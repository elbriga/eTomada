#pragma once

void rtcInit();
bool rtcAtivo();

void rtcSetSystemClock();
void rtcStoreSystemClock();

void rtcForceResetSystemTime();
