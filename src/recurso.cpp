#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"

static Recurso *recursos;
static int totRecursos = 0;

void recursosInit() {
  int recursoAddCount = 0;
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();
  int totRecursosRemotos = recursosRemotosGetCount();

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

  for (int rr=0; rr < totRecursosRemotos; rr++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    recurso->device = recursoRemotoGetPorId(rr);
    RecursoRemoto *recursoRemoto = (RecursoRemoto *)recurso->device;

    strcpy(recurso->id, recursoRemoto->id);
    recurso->tipo   = recursoRemoto->tipo;
    recurso->num    = recursoRemoto->numRR;
    recurso->remoto = true;
  }

  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r=0; r < tot; r++) {
    Recurso *recurso = &recursos[r];
    recursoPrint(recurso);
  }
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

/*int recursoSetEstado(Recurso *r, bool estado) {
  if (r->tipo != RECURSO_RELE) return 10;

  if (r->remoto) {
    // API
    NodoRemoto *nr = (NodoRemoto *)r->device;
    logaMensagem("Acionar rele remoto em %s", nr->ip.toString().c_str());
  } else {
    releControla(r->num, estado);
  }

  return 0;
}*/

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

  switch (r->tipo)
  {
  case RECURSO_RELE:
    Rele *rele;
    if (r->remoto) rele = &((RecursoRemoto *)r->device)->rele;
    else           rele = (Rele *)r->device;
    doc["device"] = releGetJSONDoc(rele, true);
    break;

  case RECURSO_SENSOR:
    Sensor *sensor;
    if (r->remoto) sensor = &((RecursoRemoto *)r->device)->sensor;
    else           sensor = (Sensor *)r->device;
    doc["device"] = sensorGetJSONDoc(sensor, true);
    break;
  
  default:
  doc["device"] = "???";
    break;
  }

  // if (full) {
  // }

  return doc;
}

void recursoPrint(Recurso *recurso) {
  logaMensagem("Recurso%s %d: %s",
      recurso->remoto ? " Remoto" : "", recurso->num,
      recurso->tipo == RECURSO_RELE ? "RELE" : (recurso->tipo == RECURSO_SENSOR ? "SENSOR" : "BOTAO"));
}
