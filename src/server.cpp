#include <ArduinoJson.h>
#include <ESPmDNS.h>

#include "eTomada.h"
#include "loga.h"
#include "server.h"
#include "recurso.h"
#include "apiInterna.h"
#include "eventos.h"
#include "mutex.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("SERVER", nivel, fmt, ##__VA_ARGS__)

void serverInit()
{
    logaM(LOG_AVISO, "eTomada Server: %s", ETOMADA_SERVER);
}

void serverEnviaEvento(String payloadJSON)
{
    String msg;
    if (!apiInternaEnviaJSON(ETOMADA_SERVER, "evento", payloadJSON, msg))
        logaM(LOG_AVISO, "Falha ao enviar evento para server [%s]", msg.c_str());
}

void serverEnviaSnapshot()
{
    String msg;
    if (!apiInternaEnviaJSON(ETOMADA_SERVER, "snapshot", eTomadaGetSnapshotJSON(), msg))
        logaM(LOG_AVISO, "Falha ao enviar snapshot para server [%s]", msg.c_str());
}
