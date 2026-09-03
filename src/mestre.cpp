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

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MESTRE", nivel, fmt, ##__VA_ARGS__)

Mestre mestre;

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
    prefs.putString("mestre1", "GROW"); // resolve por mDNS

    mestre.deviceID = getPrefsAtr(prefs, "1", "mestre");
    prefs.end();

    if (mestreAtivo())
        logaM(LOG_AVISO, "Nodo Mestre: %s", mestre.deviceID.c_str());

    mestre.online = false;
    mestreCheckOnline();
}

void mestreCheckOnline()
{
    if (!mestreAtivo())
        return;

    // Escanear
    int totND = MDNS.queryService("etomada", "tcp");

    // Procurar nosso mestre
    IPAddress ipMestre = nullptr;
    for (int nd = 0; nd < totND; nd++)
        if (MDNS.hostname(nd) == mestre.deviceID)
        {
            ipMestre = MDNS.IP(nd);
            break;
        }
    if (ipMestre)
    {
        if (!mestre.online)
            logaM(LOG_AVISO, "Mestre Online!");
        mestre.online = true;

        if (mestre.ip != ipMestre)
            logaM(LOG_AVISO, "Mestre novo IP [%s]", ipMestre.toString().c_str());
        mestre.ip = ipMestre;

        mestre.ultimoHeartbeat = millis();
    }
}

void mestreLoop()
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    if (millis() - mestre.ultimoHeartbeat > MESTRE_HEARTBEAT_TIMEOUT)
    {
        if (mestre.online)
            logaM(LOG_AVISO, "Mestre - OFFLINE!");
        mestre.online = false;
    }
}

void mestreEnviaEvento(Recurso *rec, TipoEvento tipoEvento)
{
    if (!mestreAtivo()) // Sem mestre retorna
        return;

    if (!mestre.online)
    {
        logaM(LOG_AVISO, "Mestre OFFLINE. Descartando evento [%d]", tipoEvento);
        return;
    }

    JsonDocument payload = recursoGetJSONEvento(rec, tipoEvento);

    apiInternaEnviaEvento(mestre.ip, &payload);
}

bool mestreAtivo()
{
    return (mestre.deviceID != "");
}

IPAddress mestreGetIP()
{
    return mestre.ip;
}
