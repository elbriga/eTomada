#pragma once

#include "reles.h"
#include "sensores.h"

void eTomadaInit();
void eTomadaRoleta();

String eTomadaGetSnapshotJSON();
String eTomadaGetReleString(int numRele);
String eTomadaGetRelesString();

void eTomadaSalvaRele(Rele *rele);
void eTomadaSalvaSensor(Sensor *sensor);
void eTomadaFactoryReset();

bool eTomadaPinoOutOK(int pino);
bool eTomadaPinoInOK(int pino);
