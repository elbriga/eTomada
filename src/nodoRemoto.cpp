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

std::atomic<bool> nodosRemotosRefreshRodando{false};
std::atomic<bool> nodosRemotosRefreshRequestOFF{false};

void nodosRemotosRefreshTask(void *args)
{
  char nodoID[32];
  IPAddress ip;
  bool fazRefresh;
  bool pula = false;

  nodosRemotosRefreshRodando = true;

  // for infinito sem o total fora do lock
  // reinicia nr=0 quando chega no final
  for (int nr = 0;; nr++)
  {
    if (nodosRemotosRefreshRequestOFF)
    {
      nodosRemotosRefreshRequestOFF = false;
      break;
    }

    // Com o delay aqui em cima os "continue" sempre respeitam o delay
    if (!pula)
      vTaskDelay(pdMS_TO_TICKS(100));
    pula = false;

    ip = IPAddress(); // zerar

    // Pegar os dados com LOCK
    {
      MutexLock lock(modeloMutex, "nodosRemotosRefreshTask1");
      if (!lock)
        continue;

      NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);
      if (!nodoRemoto)
      {
        nr = -1; // FORÇAR O RESET DO FOR!!!! = LOOP INFINITO
        continue;
      }

      // Nodo sem IP? nao esta cadastrado no DNS, vai nos custar 1s!
      if (!nodoRemoto->ip)
      {
        pula = true;
        continue;
      }

      uint32_t tempoDesdeUltimoContato = millis() - nodoRemoto->ultimoContato;
      if (tempoDesdeUltimoContato > 70 * 1000)
      {
        if (nodoRemoto->online)
          logaM(LOG_AVISO, "Nodo [%s] OFFLINE!", nodoRemoto->id);
        nodoRemoto->online = false;
      }

      if (millis() - nodoRemoto->ultimoTeste < 20 * 1000)
        continue;

      if (nodoRemoto->refreshPendente ||
          !nodoRemoto->online ||
          tempoDesdeUltimoContato > 30 * 1000)
      {
        nodoRemoto->ultimoTeste = millis();

        strlcpy(nodoID, nodoRemoto->id, sizeof(nodoID));
        ip = nodoRemoto->ip;

        fazRefresh = nodoRemoto->refreshPendente;
        // if (fazRefresh)
        //   logaM(LOG_NORMAL, "Fazer refresh de [%s]", nodoID);
        // else
        //   logaM(LOG_NORMAL, "Fazer PING para [%s]", nodoID);
      }
    }

    if (!ip)
      continue;

    if (fazRefresh)
    {
      String msg;
      JsonDocument snapshot;
      // HTTP sem o LOCK
      if (!apiInternaGetSnapshot(ip, snapshot, msg))
      {
        logaM(LOG_AVISO, "nodosRemotosRefreshTask > apiIntGetSnapshot > [%s]", msg.c_str());
        snapshot.clear();
        continue;
      }

      {
        MutexLock lock(modeloMutex, "nodosRemotosRefresh2");
        if (!lock)
          continue;

        NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
        if (!nodoRemoto)
          continue;

        recursoRemotoAtualizaFromSnapshotLocked(nodoRemoto, snapshot);
      }

      snapshot.clear();

      // regrasBoot fora do Lock
      bool regrasBootOK = regrasBoot(nodoID);

      {
        MutexLock lock(modeloMutex, "nodosRemotosRefreshTask3");
        if (!lock)
          continue;

        NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
        if (!nodoRemoto)
        {
          logaM(LOG_AVISO, "nodosRemotosRefreshTask :: nodo sumiu!");
          continue;
        }

        nodoRemoto->refreshPendente = !regrasBootOK;
        nodoRemoto->online = true;
        nodoRemoto->ultimoContato = millis();
        nodoRemoto->ultimoTeste = millis();
      }

      continue;
    }

    // Fazer o PING
    String msg;
    bool online = apiInternaPing(ip, msg);

    {
      MutexLock lock(modeloMutex, "nodosRemotosRefreshTask4");
      if (!lock)
        continue;

      NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
      if (!nodoRemoto)
      {
        logaM(LOG_AVISO, "nodosRemotosRefreshTask :: nodo sumiu 2!");
        continue;
      }

      nodoRemoto->ultimoTeste = millis();

      if (online)
      {
        if (!nodoRemoto->online)
        {
          logaM(LOG_AVISO, "Nodo[%s] Ficou Online!", nodoRemoto->id);
          nodoRemoto->refreshPendente = true;
          nodoRemoto->ultimoTeste = millis() - 30000UL; // para o refresh
        }

        nodoRemoto->online = true;
        nodoRemoto->ultimoContato = millis();
      }
    }
  }

  logaM(LOG_NORMAL, "Encerrando Task de Refresh");

  nodosRemotosRefreshRodando = false;
  vTaskDelete(NULL);
}

void nodosRemotosRefreshCtl(bool liga)
{
  if (!liga) // Desligar a Task
  {
    if (!nodosRemotosRefreshRodando)
      return;

    // Solicitar o desligamento
    nodosRemotosRefreshRequestOFF = true;

    // Aguardar!
    uint32_t inicio = millis();
    uint32_t t3, t3i = 0;
    while (nodosRemotosRefreshRodando)
    {
      vTaskDelay(pdMS_TO_TICKS(25));

      t3 = (millis() - inicio) / 3000;
      if (t3i != t3)
      {
        t3i = t3;
        logaM(LOG_AVISO, "AGUARDANDO encerramento da Task de refresh a [%d]s", t3 * 3);
      }
    }

    return;
  }

  if (!nodosRemotosGetCount())
  {
    logaM(LOG_AVISO, "nodosRemotosRefresh OFF > SEM NODOS REMOTOS");
    return;
  }

  if (nodosRemotosRefreshRodando)
  {
    logaM(LOG_CRITICO, "nodosRemotosRefreshCtl(ON) > JA ATIVO!!!!");
    return;
  }

  nodosRemotosRefreshRodando = true;
  nodosRemotosRefreshRequestOFF = false;

  if (xTaskCreate(
          nodosRemotosRefreshTask,
          "nrRefresh",
          8192 * 2,
          nullptr,
          1,
          NULL) != pdPASS)
  {
    logaM(LOG_CRITICO, "ERRO ao criar Task de refresh!!!");
    nodosRemotosRefreshRodando = false;
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
