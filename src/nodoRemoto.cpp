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
#include "apiInterna.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("NODORMT", nivel, fmt, ##__VA_ARGS__)

#define MAX_NOVOS_NODOS 4

NodoRemoto *nodosRemotos = nullptr;
static int totNodosRemotos = 0;

// Array para guardar o nome dos novos eTomada encontrados
struct NovoNodo
{
  bool ativo;
  char nome[32];
  IPAddress ip;
  char tipo[8];
};
static NovoNodo novosNodos[MAX_NOVOS_NODOS] = {};

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

int nodosRemotosGetNovosCount()
{
  int ret = 0;
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
    if (novosNodos[i].ativo)
      ret++;
  return ret;
}

JsonDocument nodosRemotosGetNovosJSON()
{
  JsonDocument doc;
  JsonArray nodos = doc.to<JsonArray>();

  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
  {
    if (!novosNodos[i].ativo)
      continue;

    JsonObject novoJS = nodos.add<JsonObject>();
    novoJS["id"] = novosNodos[i].nome;
    novoJS["tipo"] = novosNodos[i].tipo;
    novoJS["ip"] = novosNodos[i].ip.toString();
  }

  return doc;
}

void nodosRemotosLimpaCacheNovosNodos()
{
  memset(novosNodos, 0, sizeof(novosNodos));
}

void nodosRemotosRefreshTask(void *args)
{
  bool ehTask = args && !strncmp((char *)args, "TASK", 4);
  int totNR = nodosRemotosGetCount();

  // Escanear
  int totND = MDNS.queryService("etomada", "tcp");
  if (!totND && !ehTask)
  {
    // Tentar de novo no boot
    delay(500);
    totND = MDNS.queryService("etomada", "tcp");
  }

  // Verificar por novos nodos
  for (int nd = 0; nd < totND; nd++)
  {
    String novoIDStr = MDNS.hostname(nd);
    const char *novoID = novoIDStr.c_str();
    NodoRemoto *temNodo = nodoRemotoGet(novoID);
    if (temNodo)
      continue;

    // Ver se já demos msg para esse novo nodo
    bool jaAvisei = false;
    for (int i = 0; i < MAX_NOVOS_NODOS; i++)
      if (novosNodos[i].ativo && !strcmp(novosNodos[i].nome, novoID))
      {
        jaAvisei = true;
        break;
      }

    if (!jaAvisei)
    {
      for (int i = 0; i < MAX_NOVOS_NODOS; i++)
        if (!novosNodos[i].ativo)
        {
          novosNodos[i].ativo = true;
          strlcpy(novosNodos[i].nome, novoID, sizeof(novosNodos[i].nome));
          novosNodos[i].ip = MDNS.IP(nd);
          strlcpy(novosNodos[i].tipo, MDNS.txt(nd, "api").c_str(), sizeof(novosNodos[i].tipo));
          break;
        }

      logaM(LOG_AVISO, ">>> Novo eTomada!!! [%s] encontrado em [%s]. Avisar na interface",
            novoID, MDNS.IP(nd).toString().c_str());
      // TODO
    }
  }

  // Atualizar os nodos encontrados
  for (int nr = 0; nr < totNR; nr++)
  {
    NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);

    // Buscar este deviceID nos nodos escaneados
    IPAddress ipScan;
    String apiScan;
    for (int nd = 0; nd < totND; nd++)
    {
      if (!strcmp(MDNS.hostname(nd).c_str(), nodoRemoto->id))
      {
        ipScan = MDNS.IP(nd);
        apiScan = MDNS.txt(nd, "api");
        break;
      }
    }

    if (!ipScan)
    {
      // TODO ? msg?
      continue;
    }

    // Verificar o IP
    if (nodoRemoto->ip != ipScan)
    {
      nodoRemoto->ip = ipScan;
      logaM(LOG_AVISO, "Nodo Remoto [%s] Novo IP: %s",
            nodoRemoto->id, nodoRemoto->ip.toString().c_str());
    }
  }

  // TODO :: Fazer esse "pooling"?
  if (false)
    for (int nr = 0; nr < totNR; nr++)
    {
      NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);

      if (!nodoRemoto->ip)
        continue;

      // Atualizar os Recurso Remoto do nodo
      JsonDocument snapshot;
      if (apiInternaGetSnapshot(nodoRemoto, snapshot) != "OK")
        continue;

      recursoRemotoAtualizaFromSnapshot(nodoRemoto, snapshot);
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
    nodo->tipo = nodoRemotoGetTipoFromStr(nodoJson["tipo"].as<String>());
    strlcpy(nodo->descricao, nodoJson["desc"].as<const char *>(), sizeof(nodo->descricao));

    totNodosRemotos++;
  }

  doc.clear();

  return "OK";
}

JsonDocument nodosRemotosGetJSON()
{
  JsonDocument doc;
  JsonArray nodos = doc.to<JsonArray>();

  int totNR = nodosRemotosGetCount();
  for (int n = 0; n < totNR; n++)
  {
    NodoRemoto *nodo = nodoRemotoGetPorIndice(n);
    JsonObject nodoJS = nodos.add<JsonObject>();
    nodoJS["id"] = nodo->id;
    nodoJS["tipo"] = nodoRemotoGetTipoStr(nodo->tipo);
    nodoJS["ip"] = nodo->ip.toString();
    nodoJS["descricao"] = nodo->descricao;
  }

  return doc;
}

const char *nodoRemotoGetTipoStr(TipoNodoRemoto tipo)
{
  if (tipo == TIPO_NODO_FULL)
    return "Full";
  if (tipo == TIPO_NODO_LITE)
    return "Lite";
  return "?";
}

TipoNodoRemoto nodoRemotoGetTipoFromStr(String tipo)
{
  if (tipo == "Full")
    return TIPO_NODO_FULL;
  if (tipo == "Lite")
    return TIPO_NODO_LITE;
  return TIPO_NODO_DESCONHECIDO;
}

void nodoRemotoPrint(NodoRemoto *nodoRemoto)
{
  logaM(LOG_NORMAL, "NodoRemoto [%s] @ [%s]",
        nodoRemoto->id, nodoRemoto->ip.toString().c_str());
}
