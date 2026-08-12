#include <ArduinoJson.h>

#include "eTomada.h"
#include "mestre.h"
#include "prefs.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "apiInterna.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MESTRE", nivel, fmt, ##__VA_ARGS__)

Mestre mestre;

#define MESTRE_HEARTBEAT_TIMEOUT 30000

void mestreInit(Preferences &prefs)
{
    // Zerar tudo
    memset(&mestre, 0, sizeof(Mestre));

    if (eTomadaGetModoOperacao() != MODO_NO) // TODO : Mestre no MODO_CONTROLADOR tambem?
        return;

    // Para testes
    // prefs.putString("mestre1", "04:D3:08:A4:AE:30"); // MAC lolin

    mestre.mac = getPrefsAtr(prefs, "1", "mestre");
    if (mestreAtivo())
        logaM(LOG_AVISO, "Nodo Mestre: %s", mestre.mac.c_str());

    mestre.online = false;
}

// chamado pelo modulo discover em discoverLoop()
void mestreCheckDiscover(String mac, IPAddress ip)
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    // logaMensagem("mestreCheckDiscover > %s == %s ??", mestre.mac.c_str(), mac.c_str());

    if (mestre.mac != mac) // Nao eh o mestre
        return;

    if (!mestre.online)
    {
        mestre.online = true;
        logaM(LOG_AVISO, "Mestre - ONLINE");
    }

    if (mestre.ip != ip)
    {
        mestre.ip = ip;
        logaM(LOG_AVISO, "Mestre - Novo IP: %s", mestre.ip.toString().c_str());
    }

    mestre.ultimoHeartbeat = millis();
}

void mestreLoop()
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    if (millis() - mestre.ultimoHeartbeat > MESTRE_HEARTBEAT_TIMEOUT)
    {
        logaM(LOG_AVISO, "Mestre - OFFLINE!");
        mestre.online = false;
    }
}

void mestreEnviaEvento(Recurso *rec)
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;
    if (!mestre.online) // Mestre offline retorna
        return;

    JsonDocument payload = recursoGetJSONEvento(rec);

    apiInternaEnviaEvento(mestre.ip, &payload);
}

bool mestreAtivo()
{
    return (mestre.mac != "");
}

IPAddress mestreGetIP()
{
    return mestre.ip;
}
