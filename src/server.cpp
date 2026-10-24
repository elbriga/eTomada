#include <ArduinoJson.h>

#include "eTomada.h"
#include "loga.h"
#include "server.h"
#include "recurso.h"
#include "apiInterna.h"
#include "eventos.h"
#include "mutex.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("SERVER", nivel, fmt, ##__VA_ARGS__)

static IPAddress serverIP;
static int serverPort;

void serverInit()
{
    serverIP = IPAddress(10, 0, 0, 1);
    serverPort = 8080;

    logaM(LOG_AVISO, "eTomada Server: %s:%d", serverIP.toString().c_str(), serverPort);
}

String serverGetIPPortStr()
{
    return serverIP.toString() + ":" + String(serverPort);
}

void serverEnviaBoot()
{
    // Avisar nosso boot para o SERVER
    JsonDocument jsonBoot;
    jsonBoot["origem"] = eTomadaDeviceID();
    jsonBoot["id"] = "ETOMADA";
    jsonBoot["timestamp"] = millis();
    jsonBoot["evento"] = "BOOT";
    jsonBoot["device"]["tipo"] = "Full";
    jsonBoot["device"]["uptime"] = millis();

    String json;
    serializeJson(jsonBoot, json);

    serverEnviaEvento(json.c_str());
}

IPAddress serverGetIP()
{
    return serverIP;
}

void serverEnviaEvento(String payloadJSON)
{
    String msg;
    if (!apiInternaEnviaJSON("server", serverIP, serverPort, "evento", payloadJSON, msg))
        logaM(LOG_AVISO, "Falha ao enviar evento para server [%s]", msg.c_str());
}

void serverEnviaSnapshot()
{
    String msg;
    if (!apiInternaEnviaJSON("server", serverIP, serverPort, "snapshot", eTomadaGetSnapshotJSON(), msg))
        logaM(LOG_AVISO, "Falha ao enviar snapshot para server [%s]", msg.c_str());
}
