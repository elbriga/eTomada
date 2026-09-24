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
#include "recurso.h"

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

void nodoRemotoCalcRecursos()
{
  // Zerar
  int totNR = nodosRemotosGetCount();
  for (int n = 0; n < totNR; n++)
  {
    NodoRemoto *nr = nodoRemotoGetPorIndice(n);
    nr->recursosCount = 0;
  }

  // Contar
  int totR = recursosGetCount();
  for (int r = 0; r < totR; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (!rec->remoto)
      continue;

    if (!rec->recursoRemoto->nodo)
    {
      logaM(LOG_CRITICO, "Recurso Remoto [%s] SEM NODO!!!", rec->id);
      continue;
    }
    rec->recursoRemoto->nodo->recursosCount++;
  }
}

/**
 * Task para buscar o IP dos Nodos Remotos e buscar o snapshot
 * chamado a cada 10s
 */
void nodosRemotosRefreshTask(void *args)
{
  int totNR = nodosRemotosGetCount();

  // Escanear
  int totND = MDNS.queryService("etomada", "tcp");

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

      if (nodoRemoto->recursosCount > 0)
        nodoRemoto->refreshPendente = true;
    }
  }

  for (int nr = 0; nr < totNR; nr++)
  {
    NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);

    if (!nodoRemoto->ip)
      continue;
    if (!nodoRemoto->refreshPendente)
      continue;

    // Atualizar os Recurso Remoto do nodo
    JsonDocument snapshot;
    if (apiInternaGetSnapshot(nodoRemoto, snapshot) != "OK")
      continue;

    recursoRemotoAtualizaFromSnapshot(nodoRemoto, snapshot);
    snapshot.clear();

    nodoRemoto->refreshPendente = false;
  }

  vTaskDelete(NULL);
}

void nodosRemotosRefresh()
{
  xTaskCreate(
      nodosRemotosRefreshTask,
      "nrRefresh",
      8192 * 2,
      nullptr,
      1,
      NULL);
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
    strlcpy(nodo->descricao, nodoJson["descricao"].as<const char *>(), sizeof(nodo->descricao));

    totNodosRemotos++;
  }

  doc.clear();

  return "OK";
}

void nodoRemotoGetJS(NodoRemoto *nodo, JsonObject &obj, bool full)
{
  obj["id"] = nodo->id;
  obj["tipo"] = nodoRemotoGetTipoStr(nodo->tipo);
  obj["descricao"] = nodo->descricao;

  if (full)
    obj["ip"] = nodo->ip.toString();
}

JsonDocument nodosRemotosGetJSON(NodoRemoto *novoNodo, bool full)
{
  JsonDocument doc;
  JsonArray nodos = doc.to<JsonArray>();

  int totNR = nodosRemotosGetCount();
  for (int n = 0; n < totNR; n++)
  {
    NodoRemoto *nodo = nodoRemotoGetPorIndice(n);
    if (nodo->del)
      continue;
    JsonObject nodoJS = nodos.add<JsonObject>();
    nodoRemotoGetJS(nodo, nodoJS, full);
  }

  if (novoNodo)
  {
    // Add!
    JsonObject obj = doc.add<JsonObject>();
    nodoRemotoGetJS(novoNodo, obj, full);
  }

  return doc;
}

String nodosRemotosPersiste(NodoRemoto *novoNodo)
{
  File file = LittleFS.open("/nodosRemotos.json.tmp", "w");
  if (!file)
    return "ERRO: ao abrir nodosRemotos.json.tmp para escrita";

  JsonDocument nodos = nodosRemotosGetJSON(novoNodo, false);

  JsonDocument doc;
  doc["nodos"] = nodos;

  String out;
  if (!serializeJson(doc, out))
  {
    file.close();
    return "ERRO: nodosRemotosPersiste:serializeJson";
  }

  logaM(LOG_AVISO, "nodosRemotosPersiste: [%s]", out.c_str());

  if (!serializeJson(doc, file))
  {
    file.close();
    return "ERRO: nodosRemotosPersiste:serializeJson FILE";
  }

  file.close();

  LittleFS.rename("/nodosRemotos.json.tmp", "/nodosRemotos.json");

  logaM(LOG_NORMAL, "Nodos Remotos Salvos!");

  return "OK";
}

String nodoRemotoAddFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("nodoRemotoAddFromJSON", doc, json))
    return "JSON Invalido";

  if (doc["id"].isNull())
    return "Informe o ID!";

  const char *novoID = doc["id"].as<const char *>();
  NodoRemoto novoNodo = {};
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
    if (novosNodos[i].ativo && !strcmp(novosNodos[i].nome, novoID))
    {
      novoNodo.ip = novosNodos[i].ip;
      novoNodo.tipo = nodoRemotoGetTipoFromStr(novosNodos[i].tipo);
      break;
    }

  if (!novoNodo.ip)
    return "Novo Nodo não encontrado!";

  strlcpy(novoNodo.id, novoID, sizeof(novoNodo.id));
  if (!doc["desc"].isNull())
    strlcpy(novoNodo.descricao, doc["desc"].as<const char *>(), sizeof(novoNodo.descricao));
  else
    strlcpy(novoNodo.descricao, novoID, sizeof(novoNodo.descricao));

  doc.clear();

  String msg = nodosRemotosPersiste(&novoNodo);
  if (msg != "OK")
    return msg;

  // ReLoad config
  eTomadaLoadConfig();

  return "OK";
}

String nodoRemotoDelFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("nodoRemotoDelFromJSON", doc, json))
    return "JSON Invalido";

  if (doc["id"].isNull())
  {
    doc.clear();
    return "Informe o ID!";
  }

  NodoRemoto *nodoDel = nodoRemotoGet(doc["id"].as<const char *>());
  doc.clear();

  if (!nodoDel)
    return "Nodo Inválido";

  // Verificar se temos RecursoRemoto que são desse nodo
  int totRR = recursosRemotosGetCount();
  for (int i = 0; i < totRR; i++)
  {
    RecursoRemoto *rr = recursoRemotoGetPorIndice(i);
    if (rr->nodo == nodoDel)
      return "Nodo em Uso";
  }

  nodoDel->del = true;

  String msg = nodosRemotosPersiste(nullptr);
  if (msg != "OK")
    return msg;

  // ReLoad config
  eTomadaLoadConfig();

  return "OK";
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
