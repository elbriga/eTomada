#pragma once

enum UmidificadorEstado
{
    UMID_DESLIGADO = 0,
    UMID_POWER1 = 1,
    UMID_POWER3 = 2,
    UMID_POWER5 = 3,
};

void umidificadorInit();
bool umidificadorAtivo();
UmidificadorEstado umidificadorGetEstado();
void umidificadorSetEstado(UmidificadorEstado estado);
String umidificadorSetFromJSON(uint8_t *json);
