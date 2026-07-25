#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "reles.h"
#include "sensores.h"

static RecursoRemoto *recursosRemotos;
static int totRecursosRemotos = 0;

void recursosRemotosInit() {
  Preferences prefs;
  prefs.begin("recursosRemotos", false);

  // Para testes
  // prefs.putString("total0", "2");
  // prefs.putString("id1", "9");
  // prefs.putString("tipo1", "1");
  // prefs.putString("nodo1", "1");
  // prefs.putString("num1",  "1");
  // prefs.putString("id2", "10");
  // prefs.putString("tipo2", "1");
  // prefs.putString("nodo2", "1");
  // prefs.putString("num2",  "2");

  totRecursosRemotos = getPrefsAtr(prefs, 0, "total").toInt();
  recursosRemotos = (RecursoRemoto *)calloc(sizeof(RecursoRemoto), totRecursosRemotos);
  if (!recursosRemotos) {
    // TODO :: DIE!
  }

  for (int rr=1; rr <= totRecursosRemotos; rr++) {
    RecursoRemoto *recursoRemoto = &recursosRemotos[rr - 1];

    int id   = getPrefsAtr(prefs, rr, "id").toInt();
    int tipo = getPrefsAtr(prefs, rr, "tipo").toInt();
    int num  = getPrefsAtr(prefs, rr, "num").toInt();
    int nodo = getPrefsAtr(prefs, rr, "nodo").toInt();

    recursoRemoto->num   = num;
    recursoRemoto->numRR = rr;
    recursoRemoto->nodo  = nodoRemotoGet(nodo);

    char tipoChar;
    switch (tipo)
    {
    case RECURSO_RELE:
    {
      tipoChar = 'R';
      recursoRemoto->tipo = RECURSO_RELE;

      Rele *rele = &recursoRemoto->rele;
      releLoadFromPrefs(rele, rr, prefs);
      rele->num = num;
    }
    break;

    case RECURSO_SENSOR:
    {
      tipoChar = 'S';
      recursoRemoto->tipo = RECURSO_SENSOR;

      Sensor *sensor = &recursoRemoto->sensor;
      sensorLoadFromPrefs(sensor, rr, prefs);
      sensor->num = num;
    }
    break;
      
    default:
      tipoChar = '?';
      break;
    }

    snprintf(recursoRemoto->id, sizeof(recursoRemoto->id),
        "%c%d", tipoChar, id);
    recursoRemotoPrint(recursoRemoto);
  }

  prefs.end();
}

int recursosRemotosGetCount() {
  return totRecursosRemotos;
}

RecursoRemoto *recursoRemotoGet(const char *id) {
  int totRR = recursosRemotosGetCount();
  for (int i=0; i < totRR; i++) {
    if (!strcmp(recursosRemotos[i].id, id)) {
      return &recursosRemotos[i];
    }
  }
  return NULL;
}

RecursoRemoto *recursoRemotoGetPorId(int i) {
  if (i < 0 || i >= recursosRemotosGetCount()) {
    return NULL;
  }

  return &recursosRemotos[i];
}

JsonDocument recursoRemotoGetJSONDoc(RecursoRemoto *r, bool full) {
  JsonDocument doc;

  //doc["nome"] = r->nome;
  doc["id"]   = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);

  Rele *teste = releGet(1);
  doc["device"] = releGetJSONDoc(teste, true);

  // if (full) {
  //   doc["pino"]     = r->pino;
  //   doc["regra"]    = r->regra;
  //   doc["ativo"]    = r->ativo;
  //   doc["estado"]   = r->estado;
  //   doc["override"] = r->override;
  // }

  return doc;
}

void recursoRemotoPrint(RecursoRemoto *recursoRemoto) {
  logaMensagem("RecursoRemoto %s[%d] em %s",
      recursoGetTipoStr(recursoRemoto->tipo), recursoRemoto->num,
      recursoRemoto->nodo->ip.toString().c_str());
}
