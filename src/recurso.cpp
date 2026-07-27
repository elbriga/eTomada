#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "http.h"
#include "apiInterna.h"

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

// REQUIRE releMutex/sensorMutex locked
String recursoGetJSONString(Recurso *r) {
  String out;
  JsonDocument doc = recursoGetJSONDoc(r, true);

  serializeJson(doc, out);
  return out;
}

String recursoSetFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }
 
  String recursoID = doc["id"];
  Recurso *recurso = recursoGet(recursoID.c_str());
  if (!recurso || recurso->tipo != RECURSO_RELE) {
    return "Recurso invalido";
  }

  bool estado = (doc["estado"].as<String>() == "1");

  String ret;
  if (recurso->remoto) {
    // API
    RecursoRemoto *rr = (RecursoRemoto *)recurso->device;
    NodoRemoto    *nr = rr->nodo;
    String         id = String("R") + String(rr->num);
    ret = apiInternaSetRecurso(nr, id, estado ? "1" : "0");
  } else {
    ret = releControla(recurso->num, estado, 30 * 60); // TODO tirar o hardcoded de 30 minutos
  }

  String recursoJSON = recursoGetJSONString(recurso);
  httpEnviaEvento(recursoJSON, "sse_recurso");

  return ret;
}

Recurso *recursoGetPorId(int posicao) {
  if (posicao >= 0 && posicao < recursosGetCount()) {
    return &recursos[posicao];
  }
  return NULL;
}

Recurso *recursoGet(const char *id) {
  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r=0; r < tot; r++) {
    if (!strcmp(recursos[r].id, id)) {
      return &recursos[r];
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
  doc["id"]     = r->id;
  doc["tipo"]   = recursoGetTipoStr(r->tipo);
  doc["remoto"] = r->remoto;

  if (full) {

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
  }

  return doc;
}

String recursoAtualizaConfigFromJSON(uint8_t *json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }

  String id = doc["id"];
  Recurso *recurso = recursoGet(id.c_str());
  if (!recurso) {
    return "Recurso Invalido";
  }

  String msg;
  switch (recurso->tipo) {
  case RECURSO_RELE:
    msg = releAtualizaConfigFromJSON(recurso, doc);
    break;
  case RECURSO_SENSOR:
    msg = sensorAtualizaConfigFromJSON(recurso, doc);
    break;
  default:
    msg = "Recurso Desconhecido";
  }

  if (msg != "OK") {
    return msg;
  }

  String recursoJSON = recursoGetJSONString(recurso);
  httpEnviaEvento(recursoJSON, "sse_recurso");

  return "OK";
}

void recursoPrint(Recurso *recurso) {
  logaMensagem("Recurso%s %d: %s",
      recurso->remoto ? " Remoto" : "", recurso->num,
      recurso->tipo == RECURSO_RELE ? "RELE" : (recurso->tipo == RECURSO_SENSOR ? "SENSOR" : "BOTAO"));
}
