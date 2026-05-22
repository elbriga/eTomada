#pragma once

#include "reles.h"

void eTomadaLoadConfig();
String eTomadaGetDataJSON();
void eTomadaSalvaRele(Rele *rele);
void eTomadaFactoryReset();

bool eTomadaPinoOutOK(int pino);
bool eTomadaPinoInOK(int pino);
