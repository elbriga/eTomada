#pragma once

void displayInit();
bool displayPodeMostrar();
void displayMostraMsg(const char* msg, int timeout = 0, bool loga = true);
void displayMostraString(int x, int y, const char *msg);
