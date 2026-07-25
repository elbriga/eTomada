#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "reles.h"

Recurso *recursos;
static int totRecursos = 0;
static int totRecursosRemotos = 0;

void recursosInit() {
  int recursoAddCount = 0;
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();

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

  totRecursos = totRelesLocais + totSensoresLocais + totRecursosRemotos;
  recursos = (Recurso *)calloc(sizeof(Recurso), totRecursos);
  if (!recursos) {
    // TODO :: DIE!
  }
  
  for (int r=1; r <= totRelesLocais; r++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    snprintf(recurso->id, 8, "R%d", r);
    recurso->tipo   = RECURSO_RELE;
    recurso->num    = r;
    recurso->remoto = false;
    recurso->device = releGet(r);
  }

  for (int s=1; s <= totSensoresLocais; s++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    snprintf(recurso->id, 8, "S%d", s);
    recurso->tipo   = RECURSO_SENSOR;
    recurso->num    = s;
    recurso->remoto = false;
    recurso->device = sensorGet(s);
  }

  for (int rr=1; rr <= totRecursosRemotos; rr++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    String id   = getPrefsAtr(prefs, rr, "id");
    String tipo = getPrefsAtr(prefs, rr, "tipo");
    int numNodo = atoi(getPrefsAtr(prefs, rr, "nodo").c_str());

    char tipoChar = tipo == "1" ? 'R' : (tipo == "2" ? 'S' : 'B');
    snprintf(recurso->id, 8, "%c%s", tipoChar, id.c_str());

    recurso->tipo = tipo == "1" ? RECURSO_RELE : 
      (tipo == "2" ? RECURSO_SENSOR : RECURSO_BOTAO);
    recurso->num    = getPrefsAtr(prefs, rr, "num").toInt();
    recurso->remoto = true;

    recurso->device = nodoRemotoGet(numNodo);
    recursoPrint(recurso);
  }

  prefs.end();
}

void setRecursoID(Recurso *r, int num) {

}

int recursosGetCount(TipoRecurso tipo) {
  if (tipo == RECURSO_TODOS) {
    return totRecursos;
  }

  int ret = 0;
  int tot = recursosGetCount();
  for (int r=0; r < tot; r++) {
    if (recursos[r].tipo == tipo) {
      ret++;
    }
  }
  return ret;
}

int recursoSetEstado(Recurso *r, bool estado) {
  if (r->tipo != RECURSO_RELE) return 10;

  if (r->remoto) {
    // API
    NodoRemoto *nr = (NodoRemoto *)r->device;
    logaMensagem("Acionar rele remoto em %s", nr->ip.toString().c_str());
  } else {
    releControla(r->num, estado);
  }

  return 0;
}

Recurso *recursoGetPorId(int posicao) {
  if (posicao >= 0 && posicao < recursosGetCount()) {
    return &recursos[posicao];
  }
  return NULL;
}

Recurso *recursoGet(TipoRecurso tipo, int posicao) {
  int pos = 0;
  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r=0; r < tot; r++) {
    Recurso *recurso = &recursos[r];
    if (recurso->tipo == tipo) {
      pos++;
      if (pos == posicao)
        return recurso;
    }
  }
  return NULL;
}

const char *recursoGetTipoStr(TipoRecurso tipo) {
  switch (tipo) {
  case RECURSO_RELE:   return "RELE";
  case RECURSO_SENSOR: return "SENSOR";
  case RECURSO_BOTAO:  return "BOTAO";
  default: return "TIPORECURSODESCONHECIDO";
  }
}

JsonDocument recursoGetJSONDoc(Recurso *r, bool full) {
  JsonDocument doc;

  //doc["nome"] = r->nome;
  doc["id"]   = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);

  // if (full) {
  //   doc["pino"]     = r->pino;
  //   doc["regra"]    = r->regra;
  //   doc["ativo"]    = r->ativo;
  //   doc["estado"]   = r->estado;
  //   doc["override"] = r->override;
  // }

  return doc;
}

void recursoPrint(Recurso *recurso) {
  logaMensagem("Recurso%s %d: %s",
      recurso->remoto ? " Remoto" : "", recurso->num,
      recurso->tipo == RECURSO_RELE ? "RELE" : (recurso->tipo == RECURSO_SENSOR ? "SENSOR" : "BOTAO"));
}
