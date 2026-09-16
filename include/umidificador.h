#pragma once

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

void umidificadorInit();
bool umidificadorAtivo();
bool umidificadorFanAtivo();

void umidificadorSetEstado(UmidificadorEstado estado);
bool umidificadorFanSetEstado(UmidificadorFanEstado estado);

String umidificadorSetFromJSON(uint8_t *json);
JsonDocument umidificadorGetJSONDoc();

// TODO :: transformar o umidificador em recurso
UmidificadorEstado umidificadorGetEstado();
UmidificadorFanEstado umidificadorFanGetEstado();
