#pragma once

#include "reles.h"
#include "sensores.h"

typedef enum {
    MODO_NO = 0,
    MODO_CONTROLADOR = 1
} ModoOperacao;

typedef struct {
    const char *modelo;
    uint8_t gpioReles[8];
    uint8_t gpioBotoes[8];
} HardwareProfile;

void eTomadaInit();
ModoOperacao eTomadaGetModoOperacao();

void eTomadaRoleta();

String eTomadaGetSnapshotJSON(bool full);

String eTomadaGetReleString(int numRele);
String eTomadaGetRelesString();

void eTomadaSalvaRele(Rele *rele);
void eTomadaSalvaSensor(Sensor *sensor);
void eTomadaFactoryReset();
