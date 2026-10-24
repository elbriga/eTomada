#pragma once
#include <Arduino.h>

#define ETOMADA_DOMAIN_NAME ".etomada"

struct Recurso; // Forward declaration

void eTomadaInit0();
void eTomadaInit();
void eTomadaLoadConfig();

String eTomadaGetVersao();
String eTomadaDeviceID();
String eTomadaDeviceModel();
String eTomadaDeviceBoard();

String eTomadaGetSnapshotJSON();

void eTomadaFactoryReset();

String getMACStr();
