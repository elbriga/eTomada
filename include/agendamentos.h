#pragma once
#include <Arduino.h>

void agendamentosInit();
void agendamentosAdd(const char *recursoID, int estado, int timeoutMs);
