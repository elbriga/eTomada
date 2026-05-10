#pragma once

#include "reles.h"

void eTomadaLoadConfig();
bool eTomadaPinoOK(int pino);
String eTomadaGetDataJSON();
void eTomadaSalvaRele(Rele *rele);
void eTomadaFactoryReset();