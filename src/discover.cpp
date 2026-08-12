#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "mestre.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("DISCOVR", nivel, fmt, ##__VA_ARGS__)

#define DISCOVER_PORT 8266

static WiFiUDP discoverUdp;
static int totDiscoverNodos = 0;

static NodoRemoto discoverNodos[MAX_NODOS_REMOTOS];
// Buffer para transportar o JSON da resposta do discover para o nodosRemotosRefreshTask
JsonDocument discoverSnapshotBuffer[MAX_NODOS_REMOTOS];

void discoverTask(void *arg);
static bool discoverTaskRunning = false;

void discoverInit()
{
    discoverUdp.begin(DISCOVER_PORT);
}

bool discoverGetTaskRunning()
{
    return discoverTaskRunning;
}

int discoverGetNodosCount()
{
    return totDiscoverNodos;
}

NodoRemoto *discoverGetNodoPorIndice(int id)
{
    if (id >= 0 && id < discoverGetNodosCount())
    {
        return &discoverNodos[id];
    }
    return NULL;
}

NodoRemoto *discoverGetNodo(const char *deviceID)
{
    int tot = discoverGetNodosCount();
    for (int dn = 0; dn < tot; dn++)
    {
        NodoRemoto *nodo = &discoverNodos[dn];
        if (!strncmp(nodo->deviceID, deviceID, 32))
        {
            return nodo;
        }
    }
    return NULL;
}

JsonDocument *discoverGetNodoSnapshot(const char *deviceID)
{
    int tot = discoverGetNodosCount();
    for (int dn = 0; dn < tot; dn++)
    {
        NodoRemoto *nodo = &discoverNodos[dn];
        if (!strncmp(nodo->deviceID, deviceID, 32))
        {
            return &discoverSnapshotBuffer[dn];
        }
    }
    return NULL;
}

/**
 * Função importante dentro do nodo filho:
 * Responsável por responder ao broadcast do Controlador
 */
void discoverLoop()
{
    if (discoverTaskRunning)
    {
        return;
    }

    int len = discoverUdp.parsePacket();

    if (len <= 0)
        return;

    // Protocolo
    JsonDocument doc;
    deserializeJson(doc, discoverUdp);
    if (doc["cmd"] != "discover")
        return;

    vTaskDelay(pdMS_TO_TICKS(20));

    logaM(LOG_DEBUG, "Respondendo ao DISCOVER para %s:%d",
          discoverUdp.remoteIP().toString().c_str(),
          discoverUdp.remotePort() + 1);

    // RESPONDER ==
    discoverUdp.beginPacket(
        discoverUdp.remoteIP(),
        discoverUdp.remotePort() + 1);
    discoverUdp.print(eTomadaGetSnapshotJSON());
    discoverUdp.endPacket();

    // Verificar se é nosso mestre
    mestreCheckDiscover(doc["mac"].as<String>(), discoverUdp.remoteIP());
}

void discoverStart(bool ehTask)
{
    if (discoverTaskRunning)
    {
        return;
    }

    discoverTaskRunning = true;

    const char *args = ehTask ? "TASK" : "BOOT";
    xTaskCreate(
        discoverTask,
        "discover",
        4096,
        (void *)args,
        1,
        NULL);
}

void discoverWaitRun(bool ehTask)
{
    discoverStart(ehTask);

    long start = millis();
    while (millis() - start < 5000)
    {
        if (!discoverGetTaskRunning())
            break;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (discoverGetTaskRunning())
    {
        // TODO :: Erro!!
    }
}

/**
 * Task de 3 segundos:
 * Envia um broadcast e espera por resposta dos nodos filho
 */
void discoverTask(void *args)
{
    bool ehTask = args && !strncmp((char *)args, "TASK", 4);

    WiFiUDP replyUdp;
    IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();

    replyUdp.begin(DISCOVER_PORT + 1);

    if (!ehTask)
        logaM(LOG_NORMAL, "Enviando cmd discover para o broadcast: %s", broadcast.toString().c_str());
    discoverUdp.beginPacket(broadcast, DISCOVER_PORT);
    // TODO :: Usar classes JSON aqui
    discoverUdp.print("{\"cmd\":\"discover\",\"mac\":\"");
    discoverUdp.print(getMACStr().c_str());
    discoverUdp.print("\"}");
    discoverUdp.endPacket();

    if (!ehTask)
        logaM(LOG_NORMAL, "Aguardar por 3 segundos");
    totDiscoverNodos = 0;
    uint32_t inicio = millis();
    while (millis() - inicio < 3000)
    {
        int len = replyUdp.parsePacket();
        if (len > 0)
        {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, replyUdp);
            if (err)
            {
                logaM(LOG_AVISO, "discoverTask : Erro JSON: %s\n", err.c_str());
                continue;
            }

            totDiscoverNodos++;

            if (totDiscoverNodos > MAX_NODOS_REMOTOS)
            {
                logaM(LOG_AVISO, "IGNORANDO NODO %d!!!", totDiscoverNodos);
            }
            else
            {
                NodoRemoto *nr = &discoverNodos[totDiscoverNodos - 1];
                nr->ip = replyUdp.remoteIP();
                nr->ping = millis() - inicio;
                String mac = doc["mac"];
                strlcpy(nr->deviceID, mac.c_str(), sizeof(nr->deviceID));

                discoverSnapshotBuffer[totDiscoverNodos - 1] = doc;

                if (!ehTask)
                {
                    logaM(LOG_NORMAL, "Achei [%s] (%d ms)!",
                          replyUdp.remoteIP().toString().c_str(),
                          nr->ping);
                    /*
                    Serial.printf("Resposta[%d] de %s:\n",
                                  totDiscoverNodos, replyUdp.remoteIP().toString().c_str());
                    String s;
                    serializeJson(doc, s);
                    Serial.write(s.c_str(), s.length());
                    Serial.println();
                    */
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    replyUdp.stop();

    if (!ehTask)
        logaM(LOG_NORMAL, "Scanner completo");

    discoverTaskRunning = false;
    vTaskDelete(NULL);
}
