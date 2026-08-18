#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "eTomada.h"
#include "loga.h"
#include "nodoRemoto.h"
#include "mestre.h"
#include "ntp.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("DISCOVR", nivel, fmt, ##__VA_ARGS__)

#define DISCOVER_PORT 8266
#define DISCOVER_TEMPO_SCAN_MS 1000
#define DISCOVER_MAX_RETRIES 3

static WiFiUDP discoverSendUdp;  // Porta que o CONTROLADOR envia o discover
static WiFiUDP discoverReplyUdp; // Porta que o CONTROLADOR recebe os reply dos NÓ
static WiFiUDP discoverRecvUdp;  // Porta que o NÓ recebe o discover

static int totDiscoverNodos = 0;
static int discoverScanID = 0;

static NodoRemoto discoverNodos[MAX_NODOS_REMOTOS];
// Buffer para transportar o JSON da resposta do discover para o nodosRemotosRefreshTask
JsonDocument discoverSnapshotBuffer[MAX_NODOS_REMOTOS];

void discoverTask(void *arg);
static bool discoverTaskRunning = false;

void discoverInit()
{
    if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
    {
        // Porta efemera para enviar o broadcast
        discoverSendUdp.begin(0);
        // Abrir a porta +1 para receber as respostas
        discoverReplyUdp.begin(DISCOVER_PORT + 1);
    }
    else
    {
        // Porta do NÓ que escuta por discover
        discoverRecvUdp.begin(DISCOVER_PORT);
    }
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
void discoverLoopNo()
{
    if (discoverTaskRunning)
    {
        return;
    }

    int len = discoverRecvUdp.parsePacket();

    if (len <= 0)
        return;

    // Protocolo
    JsonDocument doc;
    deserializeJson(doc, discoverRecvUdp);
    if (doc["cmd"] != "discover")
    {
        logaM(LOG_AVISO, "discoverLoop :: cmd nao discover!");
        return;
    }

    int porta = doc["replyTo"].as<int>();
    if (porta < 1024)
    {
        logaM(LOG_AVISO, "discoverLoop :: Sem porta para reply!");
        return;
    }

    logaM(LOG_DESATIVADO, "Respondendo ao DISCOVER para %s:%d",
          discoverRecvUdp.remoteIP().toString().c_str(),
          porta);

    // TODO :: Responder somente para o mestre ou quando ainda nao temos mestre?
    // RESPONDER ==
    discoverRecvUdp.beginPacket(
        discoverRecvUdp.remoteIP(),
        porta);
    discoverRecvUdp.print(eTomadaGetSnapshotJSON());
    discoverRecvUdp.endPacket();

    // Verificar se é nosso mestre
    mestreCheckDiscover(doc["mac"].as<String>(), discoverRecvUdp.remoteIP());
}

void discoverStart(bool ehTask)
{
    if (discoverTaskRunning)
    {
        return;
    }

    discoverTaskRunning = true;

    const char *args = ehTask ? "TASK" : "BOOT";
    // logaM(LOG_DEBUG0, "discoverStart [%s]", args);

    xTaskCreate(
        discoverTask,
        "discover",
        4096,
        (void *)args,
        1,
        NULL);
}

bool discoverWaitRun(bool ehTask)
{
    if (discoverGetTaskRunning())
    {
        logaM(LOG_CRITICO, ">> discoverWaitRun >> TASK JA RODANDO!!!");
        return false;
    }

    int maxDelay = DISCOVER_TEMPO_SCAN_MS * DISCOVER_MAX_RETRIES + 200;

    discoverStart(ehTask);

    long start = millis();
    while (millis() - start < maxDelay)
    {
        if (!discoverGetTaskRunning())
            break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (discoverGetTaskRunning())
    {
        logaM(LOG_CRITICO, ">> discoverWaitRun >> TASK RODANDO MESMO DEPOIS DE maxDelay[%d]", maxDelay);
        return false;
    }
    return true;
}

/**
 * Task de DISCOVER_TEMPO_SCAN_MS ms * 3(max):
 * Envia um broadcast e espera por resposta dos nodos filho
 */
static void discoverTaskScan(int scanID, JsonDocument &doc, bool logar);
void discoverTask(void *args)
{
    bool ehTask = args && !strncmp((char *)args, "TASK", 4);
    bool logar = true; // !ehTask;

    JsonDocument doc;
    doc["app"] = "eTomada";
    doc["device"] = eTomadaDeviceID();
    doc["cmd"] = "discover";
    doc["mac"] = getMACStr();
    doc["replyTo"] = DISCOVER_PORT + 1;

    if (logaRemotoAtivo())
        doc["logServer"] = logaGetLogServer(); // TODO :: tratar nos NODO_NO

    discoverScanID++;

    int totNR = nodosRemotosGetCount();

    // Tentar achar os nodos remotos ate 3 vezes
    totDiscoverNodos = 0;
    discoverTaskScan(discoverScanID, doc, logar);
    if (totDiscoverNodos < totNR)
    {
        // Tentar mais uma vez!
        discoverTaskScan(discoverScanID, doc, logar);
        if (totDiscoverNodos < totNR)
        {
            // Tentar mais uma vez!
            discoverTaskScan(discoverScanID, doc, logar);
        }
    }

    if (logar)
        logaM(LOG_NORMAL, ">> Scan completo - Nodos encontrados: [%d]", totDiscoverNodos);

    discoverTaskRunning = false;
    vTaskDelete(NULL);
}

static void discoverTaskScan(int scanID, JsonDocument &doc, bool logar)
{
    if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
    {
        logaM(LOG_CRITICO, ">> discoverTaskScan : ABORTANDO : chamado de NO?");
        return;
    }

    IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();

    if (logar)
        logaM(LOG_DEBUG0, "Enviando cmd discover[%d] para o broadcast: %s", scanID, broadcast.toString().c_str());

    // Obter horario
    struct tm timeinfo;
    sysGetTime(&timeinfo);

    doc["time"] = mktime(&timeinfo); // TODO tratar nos nodos, se precisarem da hora
    doc["scanID"] = scanID;

    String out;
    serializeJson(doc, out);

    discoverSendUdp.beginPacket(broadcast, DISCOVER_PORT);
    discoverSendUdp.print(out.c_str());
    discoverSendUdp.endPacket();

    // if (logar)
    //     logaM(LOG_DEBUG, ">> Aguardar por %d ms", DISCOVER_TEMPO_SCAN_MS);

    esp_task_wdt_reset(); // alimenta o watchdog

    uint32_t inicio = millis();
    while (millis() - inicio < DISCOVER_TEMPO_SCAN_MS)
    {
        if (!discoverReplyUdp.parsePacket())
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (totDiscoverNodos >= MAX_NODOS_REMOTOS)
        {
            logaM(LOG_AVISO, ">> discoverTask IGNORANDO NODO!!! MAX_NODOS_REMOTOS");
            continue;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, discoverReplyUdp);
        if (err)
        {
            logaM(LOG_AVISO, ">> discoverTask : Erro JSON: %s\n", err.c_str());
            continue;
        }

        const char *novoMAC = doc["mac"].as<const char *>();

        // Verificar se já não temos resposta deste nodo no buffer
        if (totDiscoverNodos)
        {
            bool respostaDuplicada = false;
            for (int i = 0; i < totDiscoverNodos; i++)
            {
                NodoRemoto *nodoTeste = &discoverNodos[i];
                if (!strcmp(nodoTeste->deviceID, novoMAC))
                {
                    // Resposta duplicada!
                    // logaM(LOG_AVISO, "<< Resposta ao discover duplicada do nodo [%s]", nodoTeste->nome);
                    respostaDuplicada = true;
                    break;
                }
            }
            if (respostaDuplicada)
                continue;
        }

        // Adicionar esse nodo no nosso array
        NodoRemoto *nr = &discoverNodos[totDiscoverNodos];
        nr->ip = discoverReplyUdp.remoteIP();
        nr->ping = millis() - inicio;
        strlcpy(nr->deviceID, novoMAC, sizeof(nr->deviceID));

        discoverSnapshotBuffer[totDiscoverNodos] = doc;

        totDiscoverNodos++;

        if (logar)
        {
            logaM(LOG_NORMAL, ">> Achei [%s] (%d ms)!",
                  discoverReplyUdp.remoteIP().toString().c_str(),
                  nr->ping);
            /*
            Serial.printf("Resposta[%d] de %s:\n",
                          totDiscoverNodos, discoverReplyUdp.remoteIP().toString().c_str());
            String s;
            serializeJson(doc, s);
            Serial.write(s.c_str(), s.length());
            Serial.println();
            */
        }
    }
}
