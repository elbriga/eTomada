#include <atomic>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "util.h"
#include "apiInterna.h"
#include "recurso.h"
#include "mutex.h"
#include "regras.h"

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
  TipoNodoRemoto tipo;
};
static NovoNodo novosNodos[MAX_NOVOS_NODOS] = {};

NodoRemoto *nodoRemotoGetPorIndice(int i);
String nodosRemotosLoad(const char *path);

void nodoRemotoInit()
{
  totNodosRemotos = 0;

  if (!LittleFS.exists(NODOS_PATH))
  {
    logaM(LOG_AVISO, "Abortando nodoRemotoInit > Arquivo [%s] nao existe!", NODOS_PATH);
    return;
  }

  String msgLoad = nodosRemotosLoad(NODOS_PATH);
  if (msgLoad != "OK")
    logaM(LOG_AVISO, ">> nodosRemotosLoad: [%s]", msgLoad.c_str());

  // Inicializar os nodos > buscar IP
  for (int nr = 0; nr < totNodosRemotos; nr++)
  {
    NodoRemoto *nodo = nodoRemotoGetPorIndice(nr);
    String hostname = String(nodo->id) + ETOMADA_DOMAIN_NAME;
    if (!WiFi.hostByName(hostname.c_str(), nodo->ip))
    {
      nodo->ip = IPAddress();
      logaM(LOG_AVISO, "Erro de DNS para o nodo [%s]", nodo->id);
    }
    else
    {
      nodo->online = false; // Ao ficar online liga a flag nodo->refreshPendente
      nodo->ultimoContato = 0;
      nodo->ultimoTeste = millis() - 20000UL;
    }

    nodoRemotoPrint(nodo);
  }
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
    novoJS["tipo"] = novosNodos[i].tipo == TIPO_NODO_FULL ? "Full" : "Lite";
    novoJS["ip"] = novosNodos[i].ip.toString();
    novoJS["online"] = true; // Considerar online
  }

  return doc;
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

void nodoRemotoEventoBoot(const char *deviceID, const char *tipoStr, IPAddress ip)
{
  logaM(LOG_AVISO, ">>> eTomada [%s]!!! [%s] evento BOOT de [%s]",
        tipoStr, deviceID, ip.toString().c_str());

  // Verificar se é um de nossos nodos remotos cadastrados
  NodoRemoto *nr = nodoRemotoGet(deviceID);
  if (nr)
  {
    nr->refreshPendente = true;
    nr->ultimoContato = 0;
    nr->ultimoTeste = 0;
    return;
  }

  // == Novo eTomada ==

  TipoNodoRemoto tipo = nodoRemotoGetTipoFromStr(tipoStr);
  if (tipo == TIPO_NODO_DESCONHECIDO)
    logaM(LOG_CRITICO, "Novo nodo com tipo DESCONHECIDO?!? Vou deixar cadastrar!!");

  // Verificar se ja temos esse nodo no buffer de novos nodos
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
    if (novosNodos[i].ativo && !strcmp(deviceID, novosNodos[i].nome))
    {
      novosNodos[i].ip = ip;
      return;
    }

  bool addOK = false;
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
  {
    if (novosNodos[i].ativo)
      continue;

    novosNodos[i].ativo = true;
    strlcpy(novosNodos[i].nome, deviceID, sizeof(novosNodos[i].nome));
    novosNodos[i].ip = ip;
    novosNodos[i].tipo = tipo;
    addOK = true;
    break;
  }

  if (!addOK)
    logaM(LOG_CRITICO, "Erro ao add novo nodo! buffer cheio?");
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

    nodo->ip = IPAddress();

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
  {
    obj["online"] = nodo->online;
    obj["ip"] = nodo->ip.toString();
  }
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

String nodosRemotosPersisteLocked(NodoRemoto *novoNodo)
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

  String id = doc["id"];
  const char *novoID = id.c_str();

  NodoRemoto novoNodo = {};
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
    if (novosNodos[i].ativo && !strcmp(novosNodos[i].nome, novoID))
    {
      novoNodo.ip = novosNodos[i].ip;
      novoNodo.tipo = novosNodos[i].tipo;
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

  {
    MutexLock lock(modeloMutex, "nodoRemotoAddFromJSON");
    if (!lock)
      return "nodoRemotoAddFromJSON :: Lock!";

    String msg = nodosRemotosPersisteLocked(&novoNodo);
    if (msg != "OK")
      return msg;
  }

  // Remover do array de Novos Nodos
  for (int i = 0; i < MAX_NOVOS_NODOS; i++)
    if (novosNodos[i].ativo && !strcmp(novosNodos[i].nome, novoID))
      novosNodos[i].ativo = false;

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

  {
    MutexLock lock(modeloMutex, "nodoRemotoDelFromJSON");
    if (!lock)
      return "nodoRemotoDelFromJSON :: Lock!";

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

    String msg = nodosRemotosPersisteLocked(nullptr);
    if (msg != "OK")
      return msg;
  }

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
