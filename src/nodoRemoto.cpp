#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("NODORMT", nivel, fmt, ##__VA_ARGS__)

NodoRemoto *nodosRemotos = nullptr;
static int totNodosRemotos = 0;

NodoRemoto *nodoRemotoGetPorIndice(int i);
void nodosRemotosRefreshTask(void *args);
String nodosRemotosLoad(const char *path);

void nodoRemotoInit()
{
  totNodosRemotos = 0;
  nodosRemotos = nullptr;

  if (!LittleFS.exists(NODOS_PATH))
  {
    logaM(LOG_AVISO, "Abortando nodoRemotoInit > Arquivo [%s] nao existe!", NODOS_PATH);
    return;
  }

  String msgLoad = nodosRemotosLoad(NODOS_PATH);
  if (msgLoad != "OK")
    logaM(LOG_AVISO, ">> nodosRemotosLoad: [%s]", msgLoad.c_str());

  // nodosRemotosRefreshTask(nullptr);

  for (int nr = 0; nr < totNodosRemotos; nr++)
    nodoRemotoPrint(nodoRemotoGetPorIndice(nr));
}

int nodosRemotosGetCount()
{
  return totNodosRemotos;
}

NodoRemoto *nodoRemotoGet(const char *id)
{
  int tot = nodosRemotosGetCount();
  for (int nr = 0; nr < tot; nr++)
  {
    NodoRemoto *nodo = nodoRemotoGetPorIndice(nr);
    if (!strcmp(nodo->id, id))
      return &nodosRemotos[nr];
  }
  return NULL;
}

NodoRemoto *nodoRemotoGetPorIndice(int i)
{
  if (i >= 0 && i < nodosRemotosGetCount())
    return &nodosRemotos[i];
  return NULL;
}

void nodosRemotosRefresh()
{
  const char *argsFlagTask = "TASK";
  xTaskCreate(
      nodosRemotosRefreshTask,
      "nrRefresh",
      4096,
      (void *)argsFlagTask,
      1,
      NULL);
}

void nodosRemotosRefreshTask(void *args)
{
  bool ehTask = args && !strncmp((char *)args, "TASK", 4);

  // Escanear
  int totND = MDNS.queryService("etomada", "tcp");
  int totNR = nodosRemotosGetCount();

  // Verificar por novos nodos
  for (int nd = 0; nd < totND; nd++)
  {
    bool achei = false;
    for (int nr = 0; nr < totNR; nr++)
    {
      NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);
      if (!strcmp(MDNS.hostname(nd).c_str(), nodoRemoto->id))
      {
        achei = true;
        break;
      }
    }
    if (!achei)
    {
      logaM(LOG_AVISO, ">>> Novo eTomada!!! [%s] encontrado em [%s]. Avisar na interface",
            MDNS.hostname(nd).c_str(), MDNS.IP(nd).toString().c_str());
      // TODO
    }
  }

  // Atualizar os nodos encontrados
  for (int nr = 0; nr < totNR; nr++)
  {
    NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);

    // Buscar este deviceID nos nodos escaneados
    bool achei = false;
    IPAddress ipScan;
    for (int nd = 0; nd < totND; nd++)
    {
      if (!strcmp(MDNS.hostname(nd).c_str(), nodoRemoto->id))
      {
        ipScan = MDNS.IP(nd);
        achei = true;
        break;
      }
    }

    if (achei)
    {
      if (!nodoRemoto->online)
        logaM(LOG_AVISO, "Nodo Remoto [%s] ONLINE", nodoRemoto->id);
      nodoRemoto->online = true;

      if (nodoRemoto->ip != ipScan)
      {
        nodoRemoto->ip = ipScan;
        logaM(LOG_AVISO, "Nodo Remoto [%s] Novo IP: %s",
              nodoRemoto->id, nodoRemoto->ip.toString().c_str());
      }

      // Atualizar os RecursoRemoto com o snapshot do discover
      // JsonDocument *snapshot = discoverGetNodoSnapshot(nodoRemoto->mac);

      // recursoRemotoAtualizaFromSnapshot(nodoRemoto, snapshot);
    }
    else
    {
      if (nodoRemoto->online)
        logaM(LOG_AVISO, "Nodo Remoto [%s] OFFLINE", nodoRemoto->id);
      nodoRemoto->online = false;
    }
  }

  if (ehTask)
    vTaskDelete(NULL);
}

String nodosRemotosLoad(const char *path)
{
  File file = LittleFS.open(path, "r");
  if (!file)
    return "ERRO: nodosRemotosLoad > nao abriu";

  JsonDocument doc;
  DeserializationError erro = utilLeJson("nodosRemotosLoad", doc, file);
  file.close();
  if (erro)
    return "ERRO: nodosRemotosLoad > JSON nodos";

  JsonArray nodosJson = doc["nodos"].as<JsonArray>();
  int totNodos = nodosJson.size();

  if (nodosRemotos)
    delete[] nodosRemotos;

  nodosRemotos = new NodoRemoto[totNodos]();
  if (!nodosRemotos)
    utilDIE("NO new NodoRemoto! DIE!!!!!!!");

  totNodosRemotos = 0;
  for (JsonObject nodoJson : nodosJson)
  {
    if (totNodosRemotos >= totNodos)
    {
      logaM(LOG_CRITICO, "ERRO! nodosRemotosLoad > TOT > TOT ??");
      break;
    }

    NodoRemoto *nodo = &nodosRemotos[totNodosRemotos];
    if (!nodo)
    {
      logaM(LOG_CRITICO, "ERRO! nodosRemotosLoad > !NODO ??");
      continue;
    }

    strlcpy(nodo->id, nodoJson["id"].as<const char *>(), sizeof(nodo->id));
    // strlcpy(nodo->desc, nodoJson["desc"].as<const char *>(), sizeof(nodo->desc));
    nodo->online = false;

    totNodosRemotos++;
  }

  doc.clear();

  return "OK";
}

void nodoRemotoPrint(NodoRemoto *nodoRemoto)
{
  logaM(LOG_NORMAL, "NodoRemoto [%s] @ [%s]",
        nodoRemoto->id, nodoRemoto->ip.toString().c_str());
}
