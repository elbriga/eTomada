#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "reles.h"
#include "sensores.h"
#include "discover.h"
#include "tipoRecurso.h"

static RecursoRemoto *recursosRemotos;
static int totRecursosRemotos = 0;

void recursosRemotosInit()
{
  Preferences prefs;
  prefs.begin("recursosRemotos", false);

  // Para testes
  // prefs.putString("total0", "4");
  // prefs.putString("idLocal1", "R9");
  // prefs.putString("tipo1", "1");
  // prefs.putString("nodo1", "1");
  // prefs.putString("idRemoto1", "R1");
  // prefs.putString("nome1", "RREMOTO1");
  // prefs.putString("regra1", "");
  // prefs.putString("idLocal2", "R10");
  // prefs.putString("tipo2", "1");
  // prefs.putString("nodo2", "1");
  // prefs.putString("idRemoto2", "R2");
  // prefs.putString("nome2", "RREMOTO2");
  // prefs.putString("regra2", "");
  // prefs.putString("idLocal3", "S5");
  // prefs.putString("tipo3", "2");
  // prefs.putString("nodo3", "2");
  // prefs.putString("idRemoto3", "S1");
  // prefs.putString("nome3", "SREMOTO1");
  // prefs.putString("idLocal4", "S6");
  // prefs.putString("tipo4", "2");
  // prefs.putString("nodo4", "2");
  // prefs.putString("idRemoto4", "S2");
  // prefs.putString("nome4", "SREMOTO2");

  totRecursosRemotos = getPrefsAtr(prefs, 0, "total").toInt();
  recursosRemotos = new RecursoRemoto[totRecursosRemotos]();
  if (!recursosRemotos)
  {
    logaMensagem("DIE!! ERRO!!!!!! new RecursoRemoto[%d]", totRecursosRemotos);
    // TODO :: DIE!
  }

  for (int rr = 1; rr <= totRecursosRemotos; rr++)
  {
    RecursoRemoto *recursoRemoto = &recursosRemotos[rr - 1];

    String idLocal = getPrefsAtr(prefs, rr, "idLocal");
    String idRemoto = getPrefsAtr(prefs, rr, "idRemoto");
    int tipo = getPrefsAtr(prefs, rr, "tipo").toInt();
    int nodo = getPrefsAtr(prefs, rr, "nodo").toInt();

    switch (tipo)
    {
    case RECURSO_RELE:
      recursoRemoto->tipo = RECURSO_RELE;
      break;
    case RECURSO_SENSOR:
      recursoRemoto->tipo = RECURSO_SENSOR;
      break;

    default:
      logaMensagem(">>> recursoRemoto com tipo [%d] invalido!!", tipo);
      recursoRemoto->tipo = RECURSO_INVALIDO;
      break;
    }

    strcpy(recursoRemoto->idLocal, idLocal.c_str());
    strcpy(recursoRemoto->idRemoto, idRemoto.c_str());

    recursoRemoto->num = rr;
    recursoRemoto->nodo = nodoRemotoGet(nodo);

    switch (tipo)
    {
    case RECURSO_RELE:
    {
      Rele *rele = &recursoRemoto->rele;
      releLoadFromPrefs(rele, rr, prefs);
      rele->ativo = true;
      // rele->estado sera setado em nodosRemotosRefreshTask
    }
    break;

    case RECURSO_SENSOR:
    {
      Sensor *sensor = &recursoRemoto->sensor;
      sensorLoadFromPrefs(sensor, rr, prefs);
      sensor->ativo = true;
      // sensor->valor sera setado em nodosRemotosRefreshTask
    }
    break;
    }

    recursoRemotoPrint(recursoRemoto);
  }

  prefs.end();
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

void recursoRemotoSetFromSnapshot(NodoRemoto *nodo, JsonDocument *snapshot)
{
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
    if (rr->nodo != nodo)
      continue;

    JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, String(rr->idRemoto));
    if (!cacheRR)
      continue;

    JsonObject deviceRemoto = cacheRR["device"];
    if (!deviceRemoto)
      continue;

    recursoAtualizaFromJson(recurso, deviceRemoto, tsSnapshot);
  }
}

void recursoRemotoPrint(RecursoRemoto *recursoRemoto)
{
  logaMensagem("RecursoRemoto %s[%d] em %s",
               recursoGetTipoStr(recursoRemoto->tipo), recursoRemoto->num,
               recursoRemoto->nodo->ip.toString().c_str());
}
