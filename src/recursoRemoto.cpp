#include <Arduino.h>

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

static RecursoRemoto *recursosRemotos;
static int totRecursosRemotos = 0;

JsonObject recursoRemotoGetFromSnapshot(JsonDocument *snapshot, String id);

void recursosRemotosInit()
{
  Preferences prefs;
  prefs.begin("recursosRemotos", false);

  // Para testes
  // prefs.putString("total", "5");
  // prefs.putString("idLocal1", "R9");
  // prefs.putString("tipo1", "1");
  // prefs.putString("nodo1", "1");
  // prefs.putString("idRemoto1", "R1");
  // prefs.putString("nome1", "RREMOTO1");
  // prefs.putString("idLocal2", "R10");
  // prefs.putString("tipo2", "1");
  // prefs.putString("nodo2", "1");
  // prefs.putString("idRemoto2", "R2");
  // prefs.putString("nome2", "RREMOTO2");
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
  // prefs.putString("idLocal5", "B2");
  // prefs.putString("tipo5", "3");
  // prefs.putString("nodo5", "1");
  // prefs.putString("idRemoto5", "B1");
  // prefs.putString("nome5", "BREMOTO1");

  totRecursosRemotos = getPrefsAtr(prefs, "", "total").toInt();
  recursosRemotos = new RecursoRemoto[totRecursosRemotos]();
  if (!recursosRemotos)
  {
    logaMensagem("DIE!! ERRO!!!!!! new RecursoRemoto[%d]", totRecursosRemotos);
    // TODO :: DIE!
  }

  for (int rr = 1; rr <= totRecursosRemotos; rr++)
  {
    RecursoRemoto *recursoRemoto = &recursosRemotos[rr - 1];

    char num[8];
    snprintf(num, sizeof(num), "%d", rr);
    String idLocal = getPrefsAtr(prefs, num, "idLocal");
    String idRemoto = getPrefsAtr(prefs, num, "idRemoto");
    int tipo = getPrefsAtr(prefs, num, "tipo").toInt();
    int nodo = getPrefsAtr(prefs, num, "nodo").toInt();

    switch (tipo)
    {
    case RECURSO_RELE:
      recursoRemoto->tipo = RECURSO_RELE;
      break;
    case RECURSO_SENSOR:
      recursoRemoto->tipo = RECURSO_SENSOR;
      break;
    case RECURSO_BOTAO:
      recursoRemoto->tipo = RECURSO_BOTAO;
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

    // Buscar o estado remoto do recurso com o snapshot do nodosRemotosInit()
    JsonObject deviceRemoto;
    {
      JsonDocument *snapshot = discoverGetNodoSnapshot(recursoRemoto->nodo->deviceID);
      if (snapshot)
      {
        JsonObject cacheRR = recursoRemotoGetFromSnapshot(snapshot, String(recursoRemoto->idRemoto));
        if (cacheRR)
          deviceRemoto = cacheRR["device"];
      }
    }

    switch (tipo)
    {
    case RECURSO_RELE:
    {
      Rele *rele = &recursoRemoto->rele;
      rele->num = rr;
      rele->ativo = true;
      if (deviceRemoto)
        rele->estado = deviceRemoto["estado"].as<bool>();
    }
    break;

    case RECURSO_SENSOR:
    {
      Sensor *sensor = &recursoRemoto->sensor;
      sensor->num = rr;
      sensor->ativo = true;
      if (deviceRemoto)
      {
        strcpy(sensor->tipo, deviceRemoto["tipo"].as<const char *>());
        sensor->valor = deviceRemoto["valor"].as<int>();
      }
    }
    break;

    case RECURSO_BOTAO:
    {
      Botao *botao = &recursoRemoto->botao;
      botao->num = rr;
      botao->ativo = true;
      if (deviceRemoto)
        botao->estado = deviceRemoto["estado"].as<bool>();
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

void recursoRemotoAtualizaFromSnapshot(NodoRemoto *nodo, JsonDocument *snapshot)
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
  logaMensagem("RecursoRemoto[%d] %s em %s",
               recursoRemoto->num, recursoGetTipoStr(recursoRemoto->tipo),
               recursoRemoto->nodo->ip.toString().c_str());
}
