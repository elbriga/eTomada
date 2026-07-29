#pragma once

#include "reles.h"
#include "sensores.h"

struct Recurso;   // Forward declaration

typedef enum {
    MODO_NO = 0,
    MODO_CONTROLADOR = 1
} ModoOperacao;

typedef struct {
    const char *modelo;
    uint8_t gpioReles[8];
    uint8_t gpioBotoes[8];
    bool relesInvertidos;
} HardwareProfile;

void eTomadaInit();
ModoOperacao eTomadaGetModoOperacao();
const char *eTomadaGetModoOperacaoStr();

String eTomadaGetSnapshotJSON(bool full);

void eTomadaSalvaRele(Recurso *r);
void eTomadaSalvaSensor(Recurso *s);

void eTomadaRoleta();
void eTomadaFactoryReset();
