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
  // prefs.putString("id1", "9");
  // prefs.putString("tipo1", "1");
  // prefs.putString("nodo1", "1");
  // prefs.putString("num1",  "1");
  // prefs.putString("nome1", "RREMOTO1");
  // prefs.putString("regra1", "");
  // prefs.putString("ativo1", "1");
  // prefs.putString("id2", "10");
  // prefs.putString("tipo2", "1");
  // prefs.putString("nodo2", "1");
  // prefs.putString("num2",  "2");
  // prefs.putString("nome2", "RREMOTO2");
  // prefs.putString("regra2", "");
  // prefs.putString("ativo2", "1");

  totRecursosRemotos = getPrefsAtr(prefs, 0, "total").toInt();
  recursosRemotos = (RecursoRemoto *)calloc(sizeof(RecursoRemoto), totRecursosRemotos);
  if (!recursosRemotos)
  {
    // TODO :: DIE!
  }

  for (int rr = 1; rr <= totRecursosRemotos; rr++)
  {
    RecursoRemoto *recursoRemoto = &recursosRemotos[rr - 1];

    int id = getPrefsAtr(prefs, rr, "id").toInt();
    int tipo = getPrefsAtr(prefs, rr, "tipo").toInt();
    int num = getPrefsAtr(prefs, rr, "num").toInt();
    int nodo = getPrefsAtr(prefs, rr, "nodo").toInt();

    // TODO :: VALIDAR TIPO!!! esta acessando TipoRecursoChar[tipo] AQUI!
    snprintf(recursoRemoto->id, sizeof(recursoRemoto->id), "%c%d", TipoRecursoChar[tipo], id);

    recursoRemoto->num = num;
    recursoRemoto->numRR = rr;
    recursoRemoto->nodo = nodoRemotoGet(nodo);

    // TODO :: nao usar o discover > discover setar em nodoRemoto[]

    // Buscar o nodo remoto do discover que foi feito no nodoRemotoInit()
    NodoRemoto *nodoDiscover = discoverGetNodo(recursoRemoto->nodo->deviceID);
    JsonObject cacheRR;

    if (!nodoDiscover)
    {
      logaMensagem("Erro ao achar nodo do recursoRemoto %d!!", rr);
    }
    else
    {
      cacheRR = nodoGetRecursoSnapshot(nodoDiscover, String(recursoRemoto->id));
    }

    switch (tipo)
    {
    case RECURSO_RELE:
    {
      snprintf(recursoRemoto->id, sizeof(recursoRemoto->id), "R%d", id);
      recursoRemoto->tipo = RECURSO_RELE;

      Rele *rele = &recursoRemoto->rele;
      releLoadFromPrefs(rele, rr, prefs);
      rele->num = num;

      if (!cacheRR.isNull())
      {
        JsonObject releRemoto = cacheRR["device"];
        rele->estado = releRemoto["estado"].as<bool>();
      }
    }
    break;

    case RECURSO_SENSOR:
    {
      recursoRemoto->tipo = RECURSO_SENSOR;

      Sensor *sensor = &recursoRemoto->sensor;
      sensorLoadFromPrefs(sensor, rr, prefs);
      sensor->num = num;
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
    if (!strcmp(recursosRemotos[i].id, id))
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

void recursoRemotoPrint(RecursoRemoto *recursoRemoto)
{
  logaMensagem("RecursoRemoto %s[%d] em %s",
               recursoGetTipoStr(recursoRemoto->tipo), recursoRemoto->num,
               recursoRemoto->nodo->ip.toString().c_str());
}
