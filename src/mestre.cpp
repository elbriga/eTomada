#include <ArduinoJson.h>
#include <ESPmDNS.h>

#include "eTomada.h"
#include "mestre.h"
#include "prefs.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "apiInterna.h"
#include "eventos.h"
#include "wifi.h"
#include "mutex.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MESTRE", nivel, fmt, ##__VA_ARGS__)

Mestre mestre;

void mestreCheckOnline();
#define MESTRE_HEARTBEAT_TIMEOUT 30000

void mestreInit()
{
    // Zerar tudo
    memset(&mestre, 0, sizeof(Mestre));

    if (eTomadaGetModoOperacao() != MODO_NO) // TODO : Mestre no MODO_CONTROLADOR tambem?
        return;

    Preferences prefs;
    prefs.begin("eTomada", false);

    // Para testes
    // prefs.putString("mestre1", "MESTRE"); // resolve por mDNS

    mestre.deviceID = getPrefsAtr(prefs, "1", "mestre");
    prefs.end();

    if (mestreAtivo())
        logaM(LOG_AVISO, "Nodo Mestre: %s", mestre.deviceID.c_str());

    mestreCheckOnline();
}

void mestreCheckOnline()
{
    if (!mestreAtivo())
        return;

    if (WiFiGetModoAP())
        return;

    // Procurar nosso mestre
    IPAddress ipMestre = MDNS.queryHost(mestre.deviceID);
    if (ipMestre && mestre.ip != ipMestre)
    {
        logaM(LOG_AVISO, "Mestre novo IP [%s]", ipMestre.toString().c_str());
        mestre.ip = ipMestre;
    }
}

void mestreLoop()
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    mestreCheckOnline();
}

void mestreEnviaEvento(String payloadJson)
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    if (!mestre.ip)
    {
        logaM(LOG_AVISO, "Mestre sem IP! Descartando evento [%d]", payloadJson.c_str());
        // TODO :: mestre.refreshPendente = true;
        return;
    }

    String msg;
    if (!apiInternaEnviaJSON(mestre.ip, 80, "evento", payloadJson, msg))
    {
        logaM(LOG_AVISO, "Falha ao enviar evento para mestre [%s]", msg.c_str());
        // TODO :: mestre.refreshPendente = true;
        return;
    }
}

bool mestreAtivo()
{
    return (mestre.deviceID != "");
}

IPAddress mestreGetIP()
{
    return mestre.ip;
}

const char *mestreGetID()
{
    return mestre.deviceID.c_str();
}
