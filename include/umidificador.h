#pragma once

struct Recurso; // Forward declaration

enum UmidificadorEstado
{
    UMID_DESLIGADO = 0,
    UMID_POWER1 = 1,
    UMID_POWER3 = 2,
    UMID_POWER5 = 3,
};

enum UmidificadorFanEstado
{
    UMIDFAN_DESLIGADO = 0,
    UMIDFAN_POWER1 = 1,
    UMIDFAN_POWER2 = 2,
    UMIDFAN_POWER3 = 3,
};

struct Umidificador
{
    UmidificadorEstado estado;
    UmidificadorFanEstado estadoFan;
};

void umidificadorInit();
bool umidificadorAtivo();
bool umidificadorFanAtivo();

Umidificador *umidificadorGet(); // Somente 1 por eTomada

String umidificadorSetEstado(UmidificadorEstado estado);
String umidificadorFanSetEstado(UmidificadorFanEstado estado);

JsonDocument umidificadorGetJSONDoc(Recurso *r, bool full);
