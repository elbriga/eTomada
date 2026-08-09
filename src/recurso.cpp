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
#include "eventos.h"
#include "mutex.h"

static Recurso *recursos;
static int totRecursos = 0;
int recursoAddCount = 0;

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, int num, const char *idParaRecursoRemoto = nullptr);

void recursosInit()
{
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

  Preferences prefs;
  prefs.begin("recursos", false);

  // Para testes
  // prefs.putString("nomeR1", "Luz");
  // prefs.putString("nomeR2", "Humidificador");

  for (int r = 1; r <= totRelesLocais; r++)
  {
    Recurso *recurso = recursoAdd(prefs, RECURSO_RELE, r);
    recurso->rele = releGet(r);
  }

  for (int s = 1; s <= totSensoresLocais; s++)
  {
    Recurso *recurso = recursoAdd(prefs, RECURSO_SENSOR, s);
    recurso->sensor = sensorGet(s);
  }

  for (int b = 1; b <= totBotoesLocais; b++)
  {
    Recurso *recurso = recursoAdd(prefs, RECURSO_BOTAO, b);
    recurso->botao = botaoGet(b);
  }

  for (int r = 0; r < totRecursosRemotos; r++)
  {
    RecursoRemoto *rr = recursoRemotoGetPorIndice(r);
    Recurso *recurso = recursoAdd(prefs, rr->tipo, rr->num, rr->idLocal);
    recurso->recursoRemoto = rr;
  }

  prefs.end();

  int tot = recursosGetCount(RECURSO_TODOS);
  for (int r = 0; r < tot; r++)
  {
    Recurso *recurso = &recursos[r];
    recursoPrint(recurso);
  }
}

static void recursoGeraID(Recurso *r)
{
  char prefixo = '?';
  switch (r->tipo)
  {
  case RECURSO_RELE:
    prefixo = 'R';
    break;
  case RECURSO_SENSOR:
    prefixo = 'S';
    break;
  case RECURSO_BOTAO:
    prefixo = 'B';
    break;
  default:
    prefixo = 'X';
  }
  snprintf(r->id, sizeof(r->id), "%c%d", prefixo, r->num);
}

void recursoLoadFromPrefs(Recurso *r, Preferences &prefs)
{
  String nome = getPrefsAtr(prefs, r->id, "nome");
  if (nome.isEmpty())
    nome = r->id;
  strlcpy(r->nome, nome.c_str(), sizeof(r->nome));
}

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, int num, const char *idParaRecursoRemoto)
{
  if (recursoAddCount >= totRecursos)
  {
    logaMensagem("ERRO!!!!! recursoAdd(%d)!! DIE!!", recursoAddCount);
    // TODO : DIE!
    return nullptr;
  }

  Recurso *r = &recursos[recursoAddCount++];

  r->tipo = tipo;
  r->num = num;
  r->remoto = !!idParaRecursoRemoto;
  r->tsAtualizacao = millis();

  if (!r->remoto)
    // Para recursos locais > gerar o ID
    recursoGeraID(r);
  else
    // Para recursos remotos > ID vem do prefs recursosRemotos
    strlcpy(r->id, idParaRecursoRemoto, sizeof(r->id));

  recursoLoadFromPrefs(r, prefs);

  return r;
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

String recursoSetFromJSON(uint8_t *json, Recurso *&recursoOut, bool enviaMestre)
{
  recursoOut = nullptr;

  JsonDocument jsonIN;
  DeserializationError err = deserializeJson(jsonIN, json);
  if (err)
    return "JSON Invalido";

  Recurso *recurso = recursoGet(jsonIN["id"].as<const char *>());
  if (!recurso)
    return "Recurso invalido";

  if (recurso->tipo != RECURSO_RELE)
    return "Recurso nao eh RELE";

  recursoOut = recurso;

  String estado = jsonIN["estado"].as<String>();
  return recursoSet(recurso, estado, enviaMestre);
}

String recursoSetUnsafe(Recurso *recurso, bool estado, bool enviaMestre)
{
  if (recurso->tipo != RECURSO_RELE)
    return "recursoSetUnsafe: Recurso nao eh RELE";

  time_t now = 0;
  time(&now);
  recurso->tsAtualizacao = now;

  String msg;
  // TODO colocar ponteiros de funcoes em Recurso para ler e escrever, ao inves desses ifs:
  if (recurso->remoto)
  {
    // API
    msg = apiInternaSetRecurso(recurso, estado ? "1" : "0");
  }
  else
  {
    msg = releControlaUnsafe(recurso->num, estado, 30 * 60); // TODO tirar o hardcoded de 30 minutos
  }

  // anunciar: recursoEnviaSSE(a.recurso); E mestreEnviaEvento(a.recurso);
  eventoPost(EVENTO_VALOR_MUDOU, recurso, true, enviaMestre);

  return msg;
}

String recursoSet(Recurso *recurso, String estadoStr, bool enviaMestre)
{
  if (recurso->tipo != RECURSO_RELE)
    return "recursoSet: Recurso nao eh RELE";

  MutexLock lock(recursosMutex);
  if (!lock)
  {
    return "recursoSet: mutex timeout";
  }

  bool estado;
  if (estadoStr == "TOGGLE")
  {
    Rele *r = recursoGetRele(recurso);
    if (!r)
      return "recursoToggle : RELE invalido";
    estado = !r->estado;
  }
  else
  {
    estado = (estadoStr == "ON");
  }

  return recursoSetUnsafe(recurso, estado, enviaMestre);
}

void recursoEnviaSSE(Recurso *recurso)
{
  String recursoStr;
  serializeJson(recursoGetJSONDoc(recurso), recursoStr);
  httpEnviaSSE(recursoStr, "sse_recurso");
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

  doc["id"] = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);
  doc["nome"] = r->nome;
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
    // recursoEnviaSSE(recurso); em outra thread
    eventoPost(EVENTO_VALOR_MUDOU, recurso, true, true);
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

  MutexLock lock(recursosMutex, pdMS_TO_TICKS(2500));
  if (!lock)
  {
    return "mutex timeout";
  }

  bool mudou = false;

  if (!doc["nome"].isNull())
  {
    mudou = true;
    strlcpy(recurso->nome, doc["nome"].as<String>().c_str(), sizeof(recurso->nome));
  }

  if (mudou)
  {
    // recursoEnviaSSE(recurso) e mestreEnviaEvento(recurso) em outra thread
    eventoPost(EVENTO_VALOR_MUDOU, recurso, true, true);
  }

  return "OK";
}

void recursoPrint(Recurso *recurso)
{
  logaMensagem("Recurso%s %s: %s [%s]",
               recurso->remoto ? " Remoto" : "", recurso->id,
               recursoGetTipoStr(recurso->tipo), recurso->nome);
}
