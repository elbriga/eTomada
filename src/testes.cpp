#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "util.h"
#include "regras.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("TESTES", nivel, fmt, ##__VA_ARGS__)

void testaOTA();
void carregaArquivosJsonTeste();

void TESTES()
{
    // Remove e altera arquivos para testar a atualização via firmware server
    // testaOTA();

    if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
    {
        // Carrega os JSON de testes
        // carregaArquivosJsonTeste();
    }
}

void testaOTA()
{
    // Apaga o arquivo e ele deve baixar de novo:
    LittleFS.remove("/www/favicon.ico.gz");
    // Altera o arquivo e ele deve corrigir
    File fp = LittleFS.open("/www/js/api.js", FILE_APPEND);
    if (fp)
    {
        const char *teste = "aaa";
        fp.write((const uint8_t *)teste, 3);
        fp.close();
    }
    else
        logaM(LOG_AVISO, "Teste de OTA! Arquivo Faltando!?");
}

void carregaArquivosJsonTeste()
{
    logaM(LOG_AVISO, "INICIALIZANDO REGRAS FROM TESTES!!!");
    utilCopiaArquivo("/config/automacoesTeste.json", REGRAS_PATH);

    logaM(LOG_AVISO, "INICIALIZANDO NODOS REMOTOS FROM TESTES!!!");
    utilCopiaArquivo("/config/nodosRemotosTeste.json", NODOS_PATH);

    logaM(LOG_AVISO, "INICIALIZANDO RECURSOS REMOTOS FROM TESTES!!!");
    utilCopiaArquivo("/config/recursosRemotosTeste.json", RECURSOS_REMOTOS_PATH);
}
