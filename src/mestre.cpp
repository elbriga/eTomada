#include <ArduinoJson.h>

#include "eTomada.h"
#include "mestre.h"
#include "prefs.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "apiInterna.h"

Mestre mestre;

#define MESTRE_HEARTBEAT_TIMEOUT 30000

void mestreInit(Preferences &prefs)
{
    // Zerar tudo
    memset(&mestre, 0, sizeof(Mestre));

    if (eTomadaGetModoOperacao() != MODO_NO) // TODO : Mestre no MODO_CONTROLADOR tambem?
        return;

    // Para testes
    // prefs.putString("mestre0", "04:D3:08:A4:AE:30"); // MAC lolin

    mestre.mac = getPrefsAtr(prefs, 0, "mestre");
    if (mestre.mac != "")
        logaMensagem("Nodo Mestre: %s", mestre.mac.c_str());

    mestre.online = false;
}

// chamado pelo modulo discover em discoverLoop()
void mestreCheckDiscover(String mac, IPAddress ip)
{
    if (mestre.mac == "") // Sem mestre retorna
        return;

    // logaMensagem("mestreCheckDiscover > %s == %s ??", mestre.mac.c_str(), mac.c_str());

    if (mestre.mac != mac) // Nao eh o mestre
        return;

    if (!mestre.online)
    {
        mestre.online = true;
        logaMensagem("Mestre - ONLINE!");
    }

    if (mestre.ip != ip)
    {
        mestre.ip = ip;
        logaMensagem("Mestre - Novo IP: %s", mestre.ip.toString().c_str());
    }

    mestre.ultimoHeartbeat = millis();
}

void mestreLoop()
{
    if (mestre.mac == "") // Sem mestre retorna
        return;

    if (millis() - mestre.ultimoHeartbeat > MESTRE_HEARTBEAT_TIMEOUT)
    {
        logaMensagem("Mestre - OFFLINE!");
        mestre.online = false;
    }
}

void mestreEnviaEvento(Recurso *rec)
{
    if (mestre.mac == "") // Sem mestre retorna
        return;
    if (!mestre.online) // Mestre offline retorna
        return;

    JsonDocument payload = recursoGetJSONEvento(rec);

    apiInternaEnviaEvento(mestre.ip, &payload);
}
