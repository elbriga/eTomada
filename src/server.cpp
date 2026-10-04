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
    apiInternaEnviaJSON(ETOMADA_SERVER, "evento", payloadJSON);
}

void serverEnviaSnapshot()
{
    String snapshotStr = eTomadaGetSnapshotJSON();
    apiInternaEnviaJSON(ETOMADA_SERVER, "snapshot", snapshotStr);
}
