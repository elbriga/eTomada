#pragma once
#include <Arduino.h>

void agendamentosInit();
void agendamentosAdd(const char *recursoID, bool estado, int timeoutMs);
