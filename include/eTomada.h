#pragma once
#include <Arduino.h>

#define ETOMADA_DOMAIN_NAME ".etomada"

struct Recurso; // Forward declaration

typedef enum
{
    MODO_NO = 0,
    MODO_CONTROLADOR = 1,
} ModoOperacao;

void eTomadaInit0();
void eTomadaInit();
void eTomadaLoadConfig();

ModoOperacao eTomadaGetModoOperacao();
const char *eTomadaGetModoOperacaoStr();

String eTomadaGetVersao();
String eTomadaDeviceID();
String eTomadaDeviceModel();
String eTomadaDeviceBoard();

String eTomadaGetSnapshotJSON();

void eTomadaFactoryReset();

String getMACStr();
