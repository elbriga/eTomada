#pragma once

#define SENSORCHUVA_RECURSOID "HORASSECO"

void sensorChuvaInit();
bool sensorChuvaAtivo();
void sensorChuvaLoopLocked();
int sensorChuvaGetHorasSemChuva();
