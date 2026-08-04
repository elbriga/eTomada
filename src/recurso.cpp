#include <Arduino.h>

#include "eTomada.h"
#include "mestre.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "http.h"
#include "apiInterna.h"

static Recurso *recursos;
static int totRecursos = 0;

void recursosInit()
{
  int recursoAddCount = 0;
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();
  int totBotoesLocais = botoesGetCount();
  int totRecursosRemotos = recursosRemotosGetCount();

  totRecursos = totRelesLocais + totSensoresLocais + totBotoesLocais + totRecursosRemotos;
  recursos = new Recurso[totRecursos]();
  if (!recursos)
  {
    // TODO :: DIE!
  }

  for (int r = 1; r <= totRelesLocais; r++)
  {
    Recurso *recurso = &recursos[recursoAddCount++];

    snprintf(recurso->id, 8, "R%d", r);
    recurso->tipo = RECURSO_RELE;
    recurso->num = r;
    recurso->remoto = false;
    recurso->rele = releGet(r);
  }

  for (int s = 1; s <= totSensoresLocais; s++)
  {
    Recurso *recurso = &recursos[recursoAddCount++];

    snprintf(recurso->id, 8, "S%d", s);
    recurso->tipo = RECURSO_SENSOR;
    recurso->num = s;
    recurso->remoto = false;
    recurso->sensor = sensorGet(s);
  }

  for (int b = 1; b <= totBotoesLocais; b++)
  {
    Recurso *recurso = &recursos[recursoAddCount++];

    snprintf(recurso->id, 8, "B%d", b);
    recurso->tipo = RECURSO_BOTAO;
    recurso->num = b;
    recurso->remoto = false;
    recurso->botao = botaoGet(b);
  }

  for (int rr = 0; rr < totRecursosRemotos; rr++)
  {
    Recurso *recurso = &recursos[recursoAddCount++];

    recurso->recursoRemoto = recursoRemotoGetPorIndice(rr);

    strcpy(recurso->id, recurso->recursoRemoto->idLocal);
    recurso->tipo = recurso->recursoRemoto->tipo;
    recurso->num = recurso->recursoRemoto->num;
    recurso->remoto = true;
  }

  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r = 0; r < tot; r++)
  {
    Recurso *recurso = &recursos[r];
    recursoPrint(recurso);
  }
}

int recursosGetCount(TipoRecurso tipo)
{
  if (tipo == RECURSO_TODOS)
  {
    return totRecursos;
  }

  int ret = 0;
  int tot = recursosGetCount();
  for (int r = 0; r < tot; r++)
  {
    if (recursos[r].tipo == tipo)
    {
      ret++;
    }
  }
  return ret;
}

// REQUIRE recursosMutex locked
String recursoGetJSONString(Recurso *r)
{
  String out;
  JsonDocument doc = recursoGetJSONDoc(r);

  serializeJson(doc, out);
  return out;
}

String recursoSetFromJSON(uint8_t *json, JsonDocument *jsonOut)
{
  JsonDocument jsonIN;
  DeserializationError err = deserializeJson(jsonIN, json);
  if (err)
  {
    return "JSON Invalido";
  }

  String recursoID = jsonIN["id"];
  Recurso *recurso = recursoGet(recursoID.c_str());
  if (!recurso)
    return "Recurso invalido";

  if (recurso->tipo != RECURSO_RELE)
    return "Recurso nao eh RELE";

  bool estado = (jsonIN["estado"].as<String>() == "1");

  return recursoSet(recurso, estado, jsonOut);
}

String recursoSet(Recurso *recurso, bool estado, JsonDocument *jsonOut)
{
  if (recurso->tipo != RECURSO_RELE)
    return "Recurso nao eh RELE";

  String msg;
  if (recurso->remoto)
  {
    // API
    msg = apiInternaSetRecurso(recurso, estado ? "1" : "0");
  }
  else
  {
    msg = releControla(recurso->num, estado, 30 * 60); // TODO tirar o hardcoded de 30 minutos
  }

  recursoEnviaSSE(recurso, jsonOut);

  mestreEnviaEvento(recurso);

  return msg;
}

void recursoEnviaSSE(Recurso *recurso, JsonDocument *jsonOut)
{
  String recursoStr;
  if (jsonOut)
  {
    *jsonOut = recursoGetJSONDoc(recurso);
    serializeJson(*jsonOut, recursoStr);
  }
  else
  {
    JsonDocument js = recursoGetJSONDoc(recurso);
    serializeJson(js, recursoStr);
  }
  httpEnviaEvento(recursoStr, "sse_recurso");
}

Recurso *recursoGetPorIndice(int posicao)
{
  if (posicao >= 0 && posicao < recursosGetCount())
  {
    return &recursos[posicao];
  }
  return NULL;
}

Recurso *recursoGet(const char *id)
{
  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r = 0; r < tot; r++)
  {
    if (!strcmp(recursos[r].id, id))
    {
      return &recursos[r];
    }
  }
  return NULL;
}

int recursoGetNum(Recurso *recurso)
{
  return recurso->remoto ? recurso->recursoRemoto->num : recurso->num;
}

Rele *recursoGetRele(Recurso *recurso)
{
  return recurso->remoto ? &recurso->recursoRemoto->rele : recurso->rele;
}

Sensor *recursoGetSensor(Recurso *recurso)
{
  return recurso->remoto ? &recurso->recursoRemoto->sensor : recurso->sensor;
}

Botao *recursoGetBotao(Recurso *recurso)
{
  return recurso->remoto ? &recurso->recursoRemoto->botao : recurso->botao;
}

const char *recursoGetTipoStr(TipoRecurso tipo)
{
  switch (tipo)
  {
  case RECURSO_RELE:
    return "RELE";
  case RECURSO_SENSOR:
    return "SENSOR";
  case RECURSO_BOTAO:
    return "BOTAO";
  default:
    return "TIPORECURSODESCONHECIDO";
  }
}

JsonDocument recursoGetJSONDoc(Recurso *r)
{
  JsonDocument doc;

  // doc["nome"] = r->nome;
  doc["id"] = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);
  doc["remoto"] = r->remoto;

  switch (r->tipo)
  {
  case RECURSO_RELE:
    doc["device"] = releGetJSONDoc(recursoGetRele(r), true);
    break;

  case RECURSO_SENSOR:
    doc["device"] = sensorGetJSONDoc(recursoGetSensor(r), true);
    break;

  case RECURSO_BOTAO:
    doc["device"] = botaoGetJSONDoc(recursoGetBotao(r), true);
    break;

  default:
    doc["device"] = "???";
    break;
  }

  return doc;
}

// REQUIRE recursosMutex locked
JsonDocument recursoGetJSONEvento(Recurso *r)
{
  JsonDocument doc;

  doc["mac"] = getMACStr();
  doc["id"] = String(r->id);

  time_t now = 0;
  time(&now);
  doc["timestamp"] = (unsigned long)now;

  JsonDocument device;
  switch (r->tipo)
  {
  case RECURSO_RELE:
    device["estado"] = r->rele->estado;
    break;
  case RECURSO_SENSOR:
    device["valor"] = r->sensor->valor;
    break;
  case RECURSO_BOTAO:
    device["estado"] = r->botao->estado;
    break;
  }

  doc["device"] = device;

  return doc;
}

String recursoEventoRecebido(uint8_t *json)
{
  JsonDocument doc;

  DeserializationError err = deserializeJson(doc, json);
  if (err)
  {
    return "JSON Invalido";
  }

  NodoRemoto *nr = nodoRemotoGetPorMAC(doc["mac"].as<const char *>());
  if (!nr)
  {
    return "Nodo Invalido!";
  }

  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r = 0; r < tot; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (!rec->remoto)
      continue;
    if (rec->recursoRemoto->nodo->num != nr->num)
      continue;

    if (!strcmp(doc["id"].as<const char *>(), rec->recursoRemoto->idRemoto))
    {
      logaMensagem("Evento recebido! Atualizar recurso [%s]", rec->id);

      return recursoAtualizaFromJson(rec, doc["device"], doc["timestamp"].as<unsigned long>());
    }
  }

  return "Recurso nao encontrado";
}

String recursoAtualizaFromJson(Recurso *recurso, JsonDocument doc, unsigned long timestamp)
{
  // Verificar a "idade" da atualizacao
  if (timestamp <= recurso->tsAtualizacao)
  {
    return "ignorando atualização antiga";
  }

  // TODO :: LOCK!
  recurso->tsAtualizacao = timestamp;

  bool mudou = false;
  switch (recurso->tipo)
  {
  case RECURSO_RELE:
  {
    Rele *rele = recursoGetRele(recurso);
    bool novoEstado = doc["estado"].as<bool>();
    mudou = (rele->estado != novoEstado);
    rele->estado = novoEstado;
  }
  break;

  case RECURSO_SENSOR:
  {
    Sensor *sensor = recursoGetSensor(recurso);

    // Inicializar sensor?
    if (!strlen(sensor->tipo) && doc["tipo"] != "")
    {
      strcpy(sensor->tipo, doc["tipo"].as<const char *>());
    }

    int novoValor = doc["valor"].as<int>();
    mudou = (sensor->valor != novoValor);
    sensor->valor = novoValor;
  }
  break;

  case RECURSO_BOTAO:
  {
    Botao *botao = recursoGetBotao(recurso);
    bool novoEstado = doc["estado"].as<bool>();
    mudou = (botao->estado != novoEstado);
    botao->estado = novoEstado;
  }
  break;
  }

  if (mudou)
  {
    recursoEnviaSSE(recurso);
  }

  return "OK";
}

String recursoAtualizaConfigFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err)
  {
    return "JSON Invalido";
  }

  String id = doc["id"];
  Recurso *recurso = recursoGet(id.c_str());
  if (!recurso)
  {
    return "Recurso Invalido";
  }

  String msg;
  switch (recurso->tipo)
  {
  case RECURSO_RELE:
    msg = releAtualizaConfigFromJSON(recurso, doc);
    break;
  case RECURSO_SENSOR:
    msg = sensorAtualizaConfigFromJSON(recurso, doc);
    break;
  case RECURSO_BOTAO:
    msg = botaoAtualizaConfigFromJSON(recurso, doc);
    break;
  default:
    msg = "Recurso Desconhecido";
  }

  if (msg != "OK")
  {
    return msg;
  }

  recursoEnviaSSE(recurso);

  return "OK";
}

void recursoPrint(Recurso *recurso)
{
  logaMensagem("Recurso%s %s %d: %s",
               recurso->remoto ? " Remoto" : "", recurso->id, recurso->num,
               recursoGetTipoStr(recurso->tipo));
}
