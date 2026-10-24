#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "util.h"
#include "regras.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("TESTES", nivel, fmt, ##__VA_ARGS__)

void testaOTA()
{
    // Apaga o arquivo e ele deve baixar de novo:
    LittleFS.remove("/www/favicon.ico.gz");
    // Altera o arquivo e ele deve corrigir
    File fp = LittleFS.open("/www/index.html", FILE_APPEND);
    if (fp)
    {
        const char *teste = "aaa";
        fp.write((const uint8_t *)teste, 3);
        fp.close();
    }
    else
        logaM(LOG_AVISO, "Teste de OTA! Arquivo Faltando!?");
}

void TESTES()
{
    // Remove e altera arquivos para testar a atualização via firmware server
    // testaOTA();
}
