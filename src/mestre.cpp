#include <ArduinoJson.h>

#include "eTomada.h"
#include "mestre.h"
#include "prefs.h"
#include "loga.h"
#include "recurso.h"
#include "apiInterna.h"
#include "eventos.h"
#include "wifi.h"
#include "mutex.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MESTRE", nivel, fmt, ##__VA_ARGS__)

struct Mestre
{
    String deviceID;
    IPAddress ip;
    int port;
};
Mestre mestre = {};

bool mestreAtivo()
{
    return (mestre.deviceID != "");
}

void mestreInit()
{
    mestre.deviceID = "";
    mestre.ip = IPAddress();

    // Migracao mestre > SERVER
    mestre.deviceID = "SERVER";
    mestre.ip = IPAddress(10, 0, 0, 1);
    mestre.port = 8080;

    /*
    Preferences prefs;
    prefs.begin("eTomada", false);

    // Para testes
    // prefs.putString("mestre1", "MESTRE"); // resolve por DNS

    mestre.deviceID = getPrefsAtr(prefs, "1", "mestre");
    prefs.end();

    if (!mestreAtivo())
    {
        logaM(LOG_AVISO, "Sem MESTRE configurado!");
        return;
    }

    String hostname = mestre.deviceID + ETOMADA_DOMAIN_NAME;
    if (!WiFi.hostByName(hostname.c_str(), mestre.ip))
    {
        logaM(LOG_CRITICO, "Falha ao resolver o IP do MESTRE [%s.etomada]", mestre.deviceID.c_str());
        return;
    }
    if (!mestre.ip)
    {
        logaM(LOG_CRITICO, "FALHA ao resolver o IP do MESTRE [%s.etomada]", mestre.deviceID.c_str());
        return;
    }
    */

    logaM(LOG_AVISO, "Nodo Mestre: [%s] @ [%s:%d]", mestre.deviceID.c_str(), mestre.ip.toString().c_str(), mestre.port);

    // Avisar nosso boot para o mestre
    JsonDocument jsonBoot;
    jsonBoot["origem"] = eTomadaDeviceID();
    jsonBoot["id"] = "ETOMADA";
    jsonBoot["timestamp"] = millis();
    jsonBoot["evento"] = "BOOT";
    jsonBoot["device"]["tipo"] = "Full";
    jsonBoot["device"]["uptime"] = millis();
    String json;
    serializeJson(jsonBoot, json);
    mestreEnviaEvento(json.c_str());
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
    if (!apiInternaEnviaJSON(mestre.deviceID.c_str(), mestre.ip, mestre.port, "evento", payloadJson, msg))
    {
        logaM(LOG_AVISO, "Falha ao enviar evento para mestre [%s]", msg.c_str());
        // TODO :: mestre.refreshPendente = true;
        return;
    }
}

IPAddress mestreGetIP()
{
    return mestre.ip;
}

const char *mestreGetID()
{
    return mestre.deviceID.c_str();
}
