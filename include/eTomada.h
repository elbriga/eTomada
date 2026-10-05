#pragma once
#include <Arduino.h>

struct Recurso; // Forward declaration

typedef enum
{
    MODO_NO = 0,
    MODO_CONTROLADOR = 1,
    MODO_EM_OTA = 99,
} ModoOperacao;

void eTomadaInit0();
void eTomadaInit();
void eTomadaLoadConfig();

ModoOperacao eTomadaGetModoOperacao();
const char *eTomadaGetModoOperacaoStr();
void eTomadaSetEmOta();

String eTomadaGetVersao();
String eTomadaDeviceID();
String eTomadaDeviceModel();
String eTomadaDeviceBoard();

String eTomadaGetSnapshotJSON();

void eTomadaRoleta();
void eTomadaFactoryReset();

String getMACStr();
