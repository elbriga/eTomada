#pragma once

#include "reles.h"
#include "sensores.h"

void eTomadaLoadConfig();
String eTomadaGetDataJSON();
void eTomadaSalvaRele(Rele *rele);
void eTomadaSalvaSensor(Sensor *sensor);
void eTomadaFactoryReset();

bool eTomadaPinoOutOK(int pino);
bool eTomadaPinoInOK(int pino);
