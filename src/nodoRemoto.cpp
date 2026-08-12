#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "discover.h"
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
    logaM(LOG_CRITICO, "ERRO: nodoRemotoInit > Arquivo [%s] nao existe!", NODOS_PATH);
    return;
  }

  String msgLoad = nodosRemotosLoad(NODOS_PATH);
  if (msgLoad != "OK")
    logaM(LOG_AVISO, ">> nodosRemotosLoad: [%s]", msgLoad.c_str());

  nodosRemotosRefreshTask(nullptr);

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

NodoRemoto *nodoRemotoGetPorMAC(const char *mac)
{
  int tot = nodosRemotosGetCount();
  for (int nr = 0; nr < tot; nr++)
  {
    NodoRemoto *nodo = nodoRemotoGetPorIndice(nr);
    if (!strcmp(nodo->deviceID, mac))
      return &nodosRemotos[nr];
  }
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
  discoverWaitRun(ehTask);

  int totNR = nodosRemotosGetCount();
  int totND = discoverGetNodosCount();

  // Verificar por novos nodos
  for (int nd = 1; nd <= totND; nd++)
  {
    NodoRemoto *nodoDiscover = discoverGetNodoPorIndice(nd - 1);
    bool achei = false;
    for (int nr = 0; nr < totNR; nr++)
    {
      NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);
      if (!strcmp(nodoDiscover->deviceID, nodoRemoto->deviceID))
      {
        achei = true;
        break;
      }
    }
    if (!achei)
    {
      logaM(LOG_AVISO, ">>> Nodo [%s] encontrado em %s. Avisar na interface",
            nodoDiscover->deviceID, nodoDiscover->ip.toString().c_str());
      // TODO
    }
  }

  for (int nr = 0; nr < totNR; nr++)
  {
    NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);

    // Buscar este deviceID nos nodos escaneados
    NodoRemoto *nodoDescoberto = discoverGetNodo(nodoRemoto->deviceID);
    if (nodoDescoberto)
    {
      if (!nodoRemoto->online)
        logaM(LOG_AVISO, "Nodo Remoto[%s] %s - ONLINE", nodoRemoto->id, nodoRemoto->nome);
      nodoRemoto->online = true;

      if (nodoRemoto->ip != nodoDescoberto->ip)
      {
        nodoRemoto->ip = nodoDescoberto->ip;
        logaM(LOG_AVISO, "Nodo Remoto[%s] %s - Novo IP: %s",
              nodoRemoto->id, nodoRemoto->nome,
              nodoRemoto->ip.toString().c_str());
      }

      nodoRemoto->ping = nodoDescoberto->ping;

      // Atualizar os RecursoRemoto com o snapshot do discover
      JsonDocument *snapshot = discoverGetNodoSnapshot(nodoRemoto->deviceID);

      recursoRemotoAtualizaFromSnapshot(nodoRemoto, snapshot);
    }
    else
    {
      if (nodoRemoto->online)
        logaM(LOG_AVISO, "Nodo Remoto[%s] %s OFFLINE", nodoRemoto->id, nodoRemoto->nome);
      nodoRemoto->online = false;
    }
  }

  if (ehTask)
  {
    vTaskDelete(NULL);
  }
}

String nodosRemotosLoad(const char *path)
{
  File file = LittleFS.open(path, "r");
  if (!file)
    return "ERRO: nodosRemotosLoad > nao abriu";

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, file);
  file.close();
  if (erro)
    return "ERRO: nodosRemotosLoad > lendo nodos";

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
    strlcpy(nodo->nome, nodoJson["nome"].as<const char *>(), sizeof(nodo->nome));
    strlcpy(nodo->deviceID, nodoJson["mac"].as<const char *>(), sizeof(nodo->deviceID));
    nodo->online = false;

    totNodosRemotos++;
  }

  return "OK";
}

void nodoRemotoPrint(NodoRemoto *nodoRemoto)
{
  logaM(LOG_NORMAL, "NodoRemoto %s [%s] > %s > [%s] (%d ms)",
        nodoRemoto->id, nodoRemoto->nome, nodoRemoto->deviceID,
        nodoRemoto->ip.toString().c_str(),
        nodoRemoto->ping);
}
