#include <Arduino.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "rele.h"
#include "sensor.h"
#include "tipoRecurso.h"
#include "util.h"
#include "apiInterna.h"
#include "regras.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RECRMT", nivel, fmt, ##__VA_ARGS__)

static RecursoRemoto *recursosRemotos;
static int totRecursosRemotos = 0;

String recursosRemotosLoad(const char *path);
JsonObject recursoRemotoGetFromSnapshot(JsonDocument &snapshot, String id);

void recursosRemotosInit()
{
  totRecursosRemotos = 0;

  if (!LittleFS.exists(RECURSOS_REMOTOS_PATH))
  {
    logaM(LOG_AVISO, "Abortando recursosRemotosInit > Arquivo [%s] nao existe!", RECURSOS_REMOTOS_PATH);
    return;
  }

  String msg = recursosRemotosLoad(RECURSOS_REMOTOS_PATH);
  if (msg != "OK")
    logaM(LOG_CRITICO, "ERRO: recursosRemotosLoad: [%s]", msg.c_str());
}

String recursosRemotosLoad(const char *path)
{
  File file = LittleFS.open(path, "r");
  if (!file)
    return "ERRO: recursosRemotosLoad > nao abriu";

  JsonDocument doc;
  DeserializationError erro = utilLeJson("recursosRemotosLoad", doc, file);
  file.close();
  if (erro)
    return "ERRO: recursosRemotosLoad > JSON recursos";

  JsonArray recursosJson = doc["recursosRemotos"].as<JsonArray>();
  int totRR = recursosJson.size();

  if (recursosRemotos)
    delete[] recursosRemotos;

  recursosRemotos = new RecursoRemoto[totRR]();
  if (!recursosRemotos)
    utilDIE("NO new RecursoRemoto! DIE!!!!!!!");

  totRecursosRemotos = 0;
  for (JsonObject rrJson : recursosJson)
  {
    if (totRecursosRemotos >= totRR)
    {
      logaM(LOG_CRITICO, "ERRO! recursosRemotosLoad > TOT > TOT ??");
      break;
    }

    RecursoRemoto *recursoRemoto = &recursosRemotos[totRecursosRemotos];
    if (!recursoRemoto)
    {
      logaM(LOG_CRITICO, "ERRO! recursosRemotosLoad > !REC ??");
      continue;
    }

    String idLocal = rrJson["idLocal"].as<String>();
    String idRemoto = rrJson["idRemoto"].as<String>();
    String tipo = rrJson["tipo"].as<String>();
    String nodo = rrJson["nodo"].as<String>();

    recursoRemoto->tipo = recursoGetTipoFromStr(tipo);
    if (recursoRemoto->tipo == RECURSO_INVALIDO)
      logaM(LOG_CRITICO, ">>> recursoRemoto com tipo [%s] invalido!!", tipo.c_str());

    strlcpy(recursoRemoto->idLocal, idLocal.c_str(), sizeof(recursoRemoto->idLocal));
    strlcpy(recursoRemoto->idRemoto, idRemoto.c_str(), sizeof(recursoRemoto->idRemoto));

    recursoRemoto->nodo = nodoRemotoGet(nodo.c_str());

    recursoRemotoPrint(recursoRemoto);

    totRecursosRemotos++;
  }

  doc.clear();

  return "OK";
}

int recursosRemotosGetCount()
{
  return totRecursosRemotos;
}

RecursoRemoto *recursoRemotoGet(const char *id)
{
  int totRR = recursosRemotosGetCount();
  for (int i = 0; i < totRR; i++)
  {
    if (!strcmp(recursosRemotos[i].idLocal, id))
    {
      return &recursosRemotos[i];
    }
  }
  return NULL;
}

RecursoRemoto *recursoRemotoGetPorIndice(int i)
{
  if (i >= 0 && i < recursosRemotosGetCount())
  {
    return &recursosRemotos[i];
  }

  return NULL;
}

RecursoRemoto *recursoRemotoGetPorIDRemoto(NodoRemoto *nodo, const char *idRemoto)
{
  int totRR = recursosRemotosGetCount();
  for (int i = 0; i < totRR; i++)
    if (recursosRemotos[i].nodo == nodo && !strcmp(recursosRemotos[i].idRemoto, idRemoto))
      return &recursosRemotos[i];

  return NULL;
}

JsonObject recursoRemotoGetFromSnapshot(JsonDocument &snapshot, String id)
{
  JsonObject recurso;

  JsonArray recursos = snapshot["recursos"];
  for (JsonObject r : recursos)
  {
    if (r["id"].as<String>() == id)
    {
      recurso = r;
      break;
    }
  }

  return recurso;
}

void recursoRemotoAtualizaFromSnapshot(NodoRemoto *nodo, JsonDocument &snapshot)
{
  Recurso *recurso;
  RecursoRemoto *rr;

  int totRecursos = recursosGetCount();
  for (int i = 0; i < totRecursos; i++)
  {
    recurso = recursoGetPorIndice(i);
    if (!recurso->remoto)
      continue;

    rr = recurso->recursoRemoto;
    if (rr->nodo != nodo) // TODO : strcmp(mac) ao inves de testar o ponteiro?
      continue;

    JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, String(rr->idRemoto));
    if (!cacheRR)
      continue;

    JsonObject deviceRemoto = cacheRR["device"];
    if (!deviceRemoto)
      continue;

    recursoAtualizaFromJson(recurso, deviceRemoto, false);
  }
}

String recursoRemotoAddFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("recursoRemotoAddFromJSON", doc, json))
    return "JSON Invalido";

  if (doc["nodo"].isNull() || doc["idRemoto"].isNull())
    return "Informe o Nodo e o ID Remoto!";

  String nodoStr = doc["nodo"];
  String idRemotoStr = doc["idRemoto"];
  doc.clear();

  NodoRemoto *nodo = nodoRemotoGet(nodoStr.c_str());
  if (!nodo)
    return "Nodo não encontrado!";

  JsonDocument snapshot;
  apiInternaGetSnapshot(nodo, snapshot);

  JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, idRemotoStr);
  if (!cacheRR)
    return "Recurso não encontrado!";

  RecursoRemoto rr;

  rr.tipo = recursoGetTipoFromStr(cacheRR["tipo"]);
  if (!recursoSetNextID(&rr))
    return "Erro ao setar idLocal!";

  rr.nodo = nodo;
  strlcpy(rr.idRemoto, idRemotoStr.c_str(), sizeof(rr.idRemoto));

  doc.clear();

  String msg = recursosRemotosPersiste(&rr);
  if (msg != "OK")
    return msg;

  // ReLoad config
  eTomadaLoadConfig();

  return "OK";
}

String recursoRemotoDelFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("recursoRemotoDelFromJSON", doc, json))
    return "JSON Invalido";

  if (doc["nodo"].isNull() || doc["idRemoto"].isNull())
    return "Informe o Nodo e o ID Remoto!";

  String nodoStr = doc["nodo"];
  String idRemotoStr = doc["idRemoto"];
  doc.clear();

  NodoRemoto *nodo = nodoRemotoGet(nodoStr.c_str());
  if (!nodo)
    return "Nodo não encontrado!";

  RecursoRemoto *rr = recursoRemotoGetPorIDRemoto(nodo, idRemotoStr.c_str());
  if (!rr)
    return "RecursoRemoto não encontrado!";

  Regra *regra = regraGetPorRecurso(rr->idLocal);
  if (regra)
    return "Recurso em uso pela regra " + String(regra->id);

  rr->del = true;

  String msg = recursosRemotosPersiste();
  if (msg != "OK")
    return msg;

  // ReLoad config
  eTomadaLoadConfig();

  return "OK";
}

void recursoRemotoGetJS(RecursoRemoto *rr, JsonObject &obj)
{
  obj["tipo"] = recursoGetTipoStr(rr->tipo);
  obj["nodo"] = rr->nodo->id;
  obj["idLocal"] = rr->idLocal;
  obj["idRemoto"] = rr->idRemoto;
}

JsonDocument recursosRemotosGetJSON(RecursoRemoto *novo)
{
  JsonDocument doc;
  JsonArray RRs = doc.to<JsonArray>();

  int totRR = recursosRemotosGetCount();
  for (int rr = 0; rr < totRR; rr++)
  {
    RecursoRemoto *rec = recursoRemotoGetPorIndice(rr);
    if (rec->del)
      continue;
    JsonObject recJS = RRs.add<JsonObject>();
    recursoRemotoGetJS(rec, recJS);
  }

  if (novo)
  {
    // Add!
    JsonObject obj = doc.add<JsonObject>();
    recursoRemotoGetJS(novo, obj);
  }

  return doc;
}

String recursosRemotosPersiste(RecursoRemoto *novoRecurso)
{
  File file = LittleFS.open("/recursosRemotos.json.tmp", "w");
  if (!file)
    return "ERRO: ao abrir recursosRemotos.json.tmp para escrita";

  JsonDocument recursos = recursosRemotosGetJSON(novoRecurso);

  JsonDocument doc;
  doc["recursosRemotos"] = recursos;

  String out;
  if (!serializeJson(doc, out))
  {
    file.close();
    return "ERRO: recursosRemotosPersiste:serializeJson";
  }

  logaM(LOG_AVISO, "recursosRemotosPersiste: [%s]", out.c_str());

  if (!serializeJson(doc, file))
  {
    file.close();
    return "ERRO: recursosRemotosPersiste:serializeJson FILE";
  }

  file.close();

  LittleFS.rename("/recursosRemotos.json.tmp", "/recursosRemotos.json");

  logaM(LOG_NORMAL, "Recursos Remotos Salvos!");

  return "OK";
}

void recursoRemotoPrint(RecursoRemoto *recursoRemoto)
{
  logaM(LOG_NORMAL, "RecursoRemoto [%s] %s em %s",
        recursoRemoto->idLocal, recursoGetTipoStr(recursoRemoto->tipo),
        recursoRemoto->nodo ? recursoRemoto->nodo->ip.toString().c_str() : "???");
}
