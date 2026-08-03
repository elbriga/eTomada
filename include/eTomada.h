#pragma once
#include <Arduino.h>

struct Recurso; // Forward declaration

typedef enum
{
    MODO_NO = 0,
    MODO_CONTROLADOR = 1
} ModoOperacao;

void eTomadaInit();
ModoOperacao eTomadaGetModoOperacao();
const char *eTomadaGetModoOperacaoStr();

String eTomadaGetSnapshotJSON();

void eTomadaSalvaRele(Recurso *r);
void eTomadaSalvaSensor(Recurso *s);

void eTomadaRoleta();
void eTomadaFactoryReset();

uint64_t getMAC();
String getMACStr();
