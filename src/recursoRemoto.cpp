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
  // prefs.putString("total0", "2");
  prefs.putString("idLocal1", "R9");
  // prefs.putString("tipo1", "1");
  // prefs.putString("nodo1", "1");
  prefs.putString("idRemoto1", "R1");
  // prefs.putString("nome1", "RREMOTO1");
  // prefs.putString("regra1", "");
  prefs.putString("idLocal2", "R10");
  // prefs.putString("tipo2", "1");
  // prefs.putString("nodo2", "1");
  prefs.putString("idRemoto2", "R2");
  // prefs.putString("nome2", "RREMOTO2");
  // prefs.putString("regra2", "");

  totRecursosRemotos = getPrefsAtr(prefs, 0, "total").toInt();
  recursosRemotos = new RecursoRemoto[totRecursosRemotos]();
  if (!recursosRemotos)
  {
    logaMensagem("ERRO!!!!!! new RecursoRemoto[%d]", totRecursosRemotos);
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
      recursoRemoto->tipo = RECURSO_INVALIDO;
      break;
    }

    strcpy(recursoRemoto->idLocal, idLocal.c_str());
    recursoRemoto->num = rr;
    recursoRemoto->nodo = nodoRemotoGet(nodo);

    switch (tipo)
    {
    case RECURSO_RELE:
    {
      Rele *rele = &recursoRemoto->rele;
      releLoadFromPrefs(rele, rr, prefs);
      // rele->estado sera setado em nodosRemotosRefreshTask
    }
    break;

    case RECURSO_SENSOR:
    {
      Sensor *sensor = &recursoRemoto->sensor;
      sensorLoadFromPrefs(sensor, rr, prefs);
      // sensor->valor sera setado em nodosRemotosRefreshTask
    }
    break;

    default:
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

RecursoRemoto *recursoRemotoGetPorId(int i)
{
  if (i < 0 || i >= recursosRemotosGetCount())
  {
    return NULL;
  }

  return &recursosRemotos[i];
}

JsonObject recursoRemotoGetFromSnapshot(JsonDocument *snapshot, String id)
{
  JsonObject recurso;

  JsonArray recursos = (*snapshot)["recursos"];
  for (JsonObject r : recursos)
  {
    logaMensagem("&&&&&&&& %s == %s ??", r["id"].as<String>().c_str(), id.c_str());
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
  RecursoRemoto *rr;
  int totRR = recursosRemotosGetCount();
  for (int i = 0; i < totRR; i++)
  {
    rr = recursoRemotoGetPorId(i);
    if (rr->nodo != nodo)
      continue;

    JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, String(rr->idLocal));

    if (cacheRR)
      logaMensagem(">>>> nodosRemotosRefreshTask -> recursoRemotoSetFromSnapshot[%d] >> ID:%s", i, cacheRR["id"].as<const char *>());
  }
}

void recursoRemotoPrint(RecursoRemoto *recursoRemoto)
{
  logaMensagem("RecursoRemoto %s[%d] em %s",
               recursoGetTipoStr(recursoRemoto->tipo), recursoRemoto->num,
               recursoRemoto->nodo->ip.toString().c_str());
}
