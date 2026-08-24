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
#include "discover.h"
#include "tipoRecurso.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RECRMT", nivel, fmt, ##__VA_ARGS__)

static RecursoRemoto *recursosRemotos;
static int totRecursosRemotos = 0;

String recursosRemotosLoad(const char *path);
JsonObject recursoRemotoGetFromSnapshot(JsonDocument *snapshot, String id);

void recursosRemotosInit()
{
  totRecursosRemotos = 0;
  recursosRemotos = nullptr;

  if (!LittleFS.exists(RECURSOS_REMOTOS_PATH))
  {
    logaM(LOG_DEBUG, "Abortando recursosRemotosInit > Arquivo [%s] nao existe!", RECURSOS_REMOTOS_PATH);
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
  DeserializationError erro = deserializeJson(doc, file);
  file.close();
  if (erro)
    return "ERRO: recursosRemotosLoad > lendo recursos";

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

    if (tipo == "RELE")
      recursoRemoto->tipo = RECURSO_RELE;
    else if (tipo == "SENSOR")
      recursoRemoto->tipo = RECURSO_SENSOR;
    else if (tipo == "BOTAO")
      recursoRemoto->tipo = RECURSO_BOTAO;
    else
    {
      logaM(LOG_CRITICO, ">>> recursoRemoto com tipo [%s] invalido!!", tipo.c_str());
      recursoRemoto->tipo = RECURSO_INVALIDO;
    }

    strlcpy(recursoRemoto->idLocal, idLocal.c_str(), sizeof(recursoRemoto->idLocal));
    strlcpy(recursoRemoto->idRemoto, idRemoto.c_str(), sizeof(recursoRemoto->idRemoto));

    recursoRemoto->nodo = nodoRemotoGet(nodo.c_str());

    // Buscar o estado remoto do recurso com o snapshot do nodosRemotosInit()
    JsonObject deviceRemoto;
    if (!recursoRemoto->nodo)
    {
      logaM(LOG_CRITICO, "ERRO: recursosRemotosInit()[%d] sem nodo!", totRecursosRemotos);
    }
    else
    {
      JsonDocument *snapshot = discoverGetNodoSnapshot(recursoRemoto->nodo->mac);
      if (snapshot)
      {
        JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, String(recursoRemoto->idRemoto));
        if (cacheRR)
          deviceRemoto = cacheRR["device"];
      }
    }

    if (tipo == "RELE")
    {
      Rele *rele = &recursoRemoto->rele;
      rele->num = totRecursosRemotos;
      rele->ativo = true;
      if (deviceRemoto)
        rele->estado = deviceRemoto["estado"].as<bool>();
    }
    else if (tipo == "SENSOR")
    {
      Sensor *sensor = &recursoRemoto->sensor;
      sensor->num = totRecursosRemotos;
      sensor->ativo = true;
      if (deviceRemoto)
      {
        strlcpy(sensor->tipo, deviceRemoto["tipo"].as<const char *>(), sizeof(sensor->tipo));
        sensor->valor = deviceRemoto["valor"].as<int>();
      }
    }
    else if (tipo == "BOTAO")
    {
      Botao *botao = &recursoRemoto->botao;
      botao->num = totRecursosRemotos;
      botao->ativo = true;
      if (deviceRemoto)
        botao->estado = deviceRemoto["estado"].as<bool>();
    }

    recursoRemotoPrint(recursoRemoto);

    totRecursosRemotos++;
  }

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

JsonObject recursoRemotoGetFromSnapshot(JsonDocument *snapshot, String id)
{
  JsonObject recurso;

  JsonArray recursos = (*snapshot)["recursos"];
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

void recursoRemotoAtualizaFromSnapshot(NodoRemoto *nodo, JsonDocument *snapshot)
{
  if (!snapshot)
  {
    logaM(LOG_CRITICO,
          "recursoRemotoAtualizaFromSnapshot: snapshot NULL para [%s]",
          nodo ? nodo->mac : "(null)");
    return;
  }

  Recurso *recurso;
  RecursoRemoto *rr;
  time_t tsSnapshot = (*snapshot)["timestamp"].as<unsigned long>();

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

    recursoAtualizaFromJson(recurso, deviceRemoto, tsSnapshot, "");
  }
}

void recursoRemotoPrint(RecursoRemoto *recursoRemoto)
{
  logaM(LOG_NORMAL, "RecursoRemoto[%s] %s em %s",
        recursoRemoto->idLocal, recursoGetTipoStr(recursoRemoto->tipo),
        recursoRemoto->nodo->ip.toString().c_str());
}
