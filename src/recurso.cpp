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
#include "agendamentos.h"
#include "util.h"
#include "umidificador.h"
#include "eventos.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RECURSO", nivel, fmt, ##__VA_ARGS__)

static Recurso *recursos;
static int totRecursos = 0;
int recursoAddCount = 0;

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, const char *id, bool remoto = false);

void recursosInit()
{
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();
  int totBotoesLocais = botoesGetCount();
  int totRecursosRemotos = recursosRemotosGetCount();

  totRecursos = totRelesLocais + totSensoresLocais + totBotoesLocais + totRecursosRemotos +
                (umidificadorAtivo() ? 1 : 0);

  if (recursos)
    delete[] recursos;

  recursos = new Recurso[totRecursos]();
  if (!recursos)
    utilDIE("RECURSOS");

  Preferences prefs;
  prefs.begin("recursos", false);

  // Para testes
  // prefs.putString("nomeR1", "Luz");
  // prefs.putString("nomeR2", "Humidificador");

  recursoAddCount = 0;
  for (int r = 1; r <= totRelesLocais; r++)
  {
    String id = "R" + String(r);
    Recurso *recurso = recursoAdd(prefs, RECURSO_RELE, id.c_str());
    recurso->rele = releGet(r);
  }

  for (int s = 1; s <= totSensoresLocais; s++)
  {
    String id = "S" + String(s);
    Recurso *recurso = recursoAdd(prefs, RECURSO_SENSOR, id.c_str());
    recurso->sensor = sensorGet(s);
  }

  for (int b = 1; b <= totBotoesLocais; b++)
  {
    String id = "B" + String(b);
    Recurso *recurso = recursoAdd(prefs, RECURSO_BOTAO, id.c_str());
    recurso->botao = botaoGet(b);
  }

  for (int r = 0; r < totRecursosRemotos; r++)
  {
    RecursoRemoto *rr = recursoRemotoGetPorIndice(r);
    Recurso *recurso = recursoAdd(prefs, rr->tipo, rr->idLocal, true);
    recurso->recursoRemoto = rr;
  }

  if (umidificadorAtivo())
  {
    Recurso *r = recursoAdd(prefs, RECURSO_UMIDIFICADOR, "UMIDIFICADOR", false);
    r->umid = umidificadorGet();
  }

  prefs.end();

  // Inicializa nodo->recursosCount
  nodoRemotoCalcRecursos();

  int tot = recursosGetCount();
  for (int r = 0; r < tot; r++)
  {
    Recurso *recurso = &recursos[r];
    recursoPrint(recurso);
  }
}

void recursoLoadFromPrefs(Recurso *r, Preferences &prefs)
{
  String nome = getPrefsAtr(prefs, r->id, "nome");
  if (nome.isEmpty())
    nome = r->id;
  strlcpy(r->nome, nome.c_str(), sizeof(r->nome));
}

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, const char *id, bool remoto)
{
  if (recursoAddCount >= totRecursos)
    utilDIE("recursoAdd!! DIE!!");

  Recurso *r = &recursos[recursoAddCount++];

  r->tipo = tipo;
  r->remoto = remoto;

  strlcpy(r->id, id, sizeof(r->id));

  if (strcmp(id, "UMIDIFICADOR"))
    recursoLoadFromPrefs(r, prefs);

  return r;
}

int recursosGetCount()
{
  return totRecursos;
}

void recursosZera()
{
  totRecursos = 0;
}

String recursoSetFromJSON(uint8_t *json, String &recursoIDOut, bool enviaMestre)
{
  recursoIDOut = "";

  JsonDocument jsonIN;
  if (utilLeJson("recursoSetFromJSON", jsonIN, json))
    return "JSON Invalido";

  String id = jsonIN["id"].as<String>();
  String estado = jsonIN["estado"].as<String>();
  String estadoFan = jsonIN["estadoFan"].as<String>();
  jsonIN.clear();

  Recurso *recurso = recursoGet(id.c_str());
  if (!recurso)
    return "Recurso invalidooo!";

  recursoIDOut = id;

  if (estadoFan != "" && estadoFan != "null")
    estado += ":" + estadoFan;
  return recursoSet(id.c_str(), estado, enviaMestre);
}

String recursoSetLocalLocked(Recurso *recurso, String estado, bool enviaMestre)
{
  if (recurso->tipo != RECURSO_RELE && recurso->tipo != RECURSO_UMIDIFICADOR)
    return "Erro recursoSetLocalLocked: Recurso nao eh RELE nem UMID";
  if (recurso->remoto)
    return "Erro recursoSetLocalLocked: recurso remoto!";

  String msg;
  // TODO colocar ponteiros de funcoes em Recurso para ler e escrever, ao inves desses ifs:
  {
    switch (recurso->tipo)
    {
    case RECURSO_RELE:
    {
      msg = releControlaLocked(recurso->rele, estado == "ON");
    }
    break;

    case RECURSO_UMIDIFICADOR:
    {
      UmidificadorEstado umidEstado = (UmidificadorEstado)estado.toInt();
      if (umidEstado < UMID_DESLIGADO || umidEstado > UMID_POWER5)
        return "resursoSetLocked: Estado UMID invalido";

      int temEstadoFan = estado.indexOf(':');
      if (temEstadoFan >= 0)
      {
        UmidificadorFanEstado estadoFan = (UmidificadorFanEstado)estado.substring(temEstadoFan + 1).toInt();
        umidificadorFanSetEstado(estadoFan);
      }
      msg = umidificadorSetEstado(umidEstado);
    }
    break;
    }
  }

  return msg;
}

String recursoSet(const char *recursoID, String estado, bool enviaMestre)
{
  String msg = "OK";

  // Buffer dos dados para não ficar com o Lock durante HTTP
  bool remoto = false;
  IPAddress ip;
  TipoNodoRemoto tipoNodo;
  char idRemoto[32];

  String estadoFinal = estado;
  {
    MutexLock lock(recursosMutex);
    if (!lock)
      return "recursoSet: mutex timeout";

    Recurso *recurso = recursoGet(recursoID);
    if (!recurso)
      return "recursoSet: recurso invalido";

    if (recurso->tipo != RECURSO_RELE && recurso->tipo != RECURSO_UMIDIFICADOR)
      return "recursoSet: Recurso nao eh RELE nem UMID";

    if (recurso->tipo == RECURSO_RELE)
    {
      if (estado == "TOGGLE")
      {
        Rele *r = recursoGetRele(recurso);
        if (!r)
          return "recursoToggle : RELE invalido";
        estadoFinal = !r->estado ? "ON" : "OFF";
      }
      else if (estado == "PULSE")
      {
        estadoFinal = "ON";
      }
      else
      {
        estadoFinal = (estado == "ON") ? "ON" : "OFF";
      }
    }

    remoto = recurso->remoto;
    if (!recurso->remoto)
    {
      // Recursos locais: Tratar dentro do Lock
      msg = recursoSetLocalLocked(recurso, estadoFinal, enviaMestre);
    }
    else
    {
      // Recursos remotos: guardar copia e soltar o Lock
      ip = recurso->recursoRemoto->nodo->ip;
      tipoNodo = recurso->recursoRemoto->nodo->tipo;
      strlcpy(idRemoto, recurso->recursoRemoto->idRemoto, sizeof(idRemoto));
    }
  }

  if (remoto)
  {
    // API
    JsonDocument resposta;
    msg = apiInternaSetRecurso(ip, tipoNodo, idRemoto, estadoFinal, resposta);

    if (!resposta.isNull())
    {
      String out;
      serializeJson(resposta, out);
      logaM(LOG_AVISO, "ATUALIZAR RECURSO REMOTO com Resposta :::::::: [%s]", out.c_str());

      MutexLock lock(recursosMutex);
      if (!lock)
        return "recursoSet: mutex timeout";

      Recurso *recurso = recursoGet(recursoID);
      if (!recurso)
        return "recursoSet: recurso sumiu!";

      switch (recurso->tipo)
      {
      case RECURSO_RELE:
      {
        Rele *rele = recursoGetRele(recurso);
        rele->estado = resposta["recurso"]["device"]["estado"].as<bool>();
      }
      break;

      case RECURSO_UMIDIFICADOR:
      {
        Umidificador *umid = recursoGetUmidificador(recurso);
        umid->estado = (UmidificadorEstado)resposta["recurso"]["device"]["estado"].as<int>();
        umid->estadoFan = (UmidificadorFanEstado)resposta["recurso"]["device"]["estadoFan"].as<int>();
      }
      break;
      }
    }
  }

  if (estado == "PULSE")
  {
    // Agendar o OFF = pulso de 1000ms
    agendamentosAdd(AGEND_RECURSO, 1000, recursoID, false);
  }

  eventoPost(EVENTO_VALOR_MUDOU, recursoID, true, enviaMestre);

  return msg;
}

void recursoEnviaSSE(const char *recursoID)
{
  String recursoStr;
  {
    MutexLock lock(recursosMutex);
    if (!lock)
    {
      logaM(LOG_CRITICO, "recursoEnviaSSE - Erro de Lock!");
      return;
    }

    Recurso *recurso = recursoGet(recursoID);
    if (!recurso)
    {
      logaM(LOG_CRITICO, "recursoEnviaSSE - Erro Recurso[%s] Invalido!", recursoID);
      return;
    }

    serializeJson(recursoGetJSONDoc(recurso), recursoStr);
  }
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
  int tot = recursosGetCount();
  for (int r = 0; r < tot; r++)
  {
    if (!strcmp(recursos[r].id, id))
    {
      return &recursos[r];
    }
  }
  return NULL;
}

Rele *recursoGetRele(Recurso *recurso)
{
  if (recurso->tipo != RECURSO_RELE)
    return nullptr;
  return recurso->remoto ? &recurso->recursoRemoto->rele : recurso->rele;
}

Sensor *recursoGetSensor(Recurso *recurso)
{
  if (recurso->tipo != RECURSO_SENSOR)
    return nullptr;
  return recurso->remoto ? &recurso->recursoRemoto->sensor : recurso->sensor;
}

Botao *recursoGetBotao(Recurso *recurso)
{
  if (recurso->tipo != RECURSO_BOTAO)
    return nullptr;
  return recurso->remoto ? &recurso->recursoRemoto->botao : recurso->botao;
}

Umidificador *recursoGetUmidificador(Recurso *recurso)
{
  if (recurso->tipo != RECURSO_UMIDIFICADOR)
    return nullptr;
  return recurso->remoto ? &recurso->recursoRemoto->umid : recurso->umid;
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
  case RECURSO_UMIDIFICADOR:
    return "UMIDIFICADOR";
  default:
    return "TIPORECURSODESCONHECIDO";
  }
}

char recursoGetTipoLetra(TipoRecurso tipo)
{
  switch (tipo)
  {
  case RECURSO_RELE:
    return 'R';
  case RECURSO_SENSOR:
    return 'S';
  case RECURSO_BOTAO:
    return 'B';
  case RECURSO_UMIDIFICADOR:
    return 'U';
  default:
    return '0';
  }
}

bool recursoSetNextID(RecursoRemoto *rr)
{
  char letra = recursoGetTipoLetra(rr->tipo);
  char id[32] = {0};
  Recurso *rec = nullptr;

  for (int r = 1; r < 1000; r++)
  {
    sprintf(id, "%c%d", letra, r);
    rec = recursoGet(id);
    if (!rec)
      break;
  }
  if (rec)
  {
    logaM(LOG_AVISO, "Mais de 1000 recursos?? Oha!");
    return false;
  }

  strlcpy(rr->idLocal, id, sizeof(rr->idLocal));
  return true;
}

TipoRecurso recursoGetTipoFromStr(String tipoStr)
{
  if (tipoStr == "RELE")
    return RECURSO_RELE;
  if (tipoStr == "SENSOR")
    return RECURSO_SENSOR;
  if (tipoStr == "BOTAO")
    return RECURSO_BOTAO;
  if (tipoStr == "UMIDIFICADOR")
    return RECURSO_UMIDIFICADOR;

  return RECURSO_INVALIDO;
}

JsonDocument recursoGetJSONDoc(Recurso *r)
{
  JsonDocument doc;

  doc["id"] = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);
  doc["nome"] = r->nome;
  doc["remoto"] = r->remoto;
  doc["nodo"] = r->remoto ? r->recursoRemoto->nodo->id : "_LOCAL";
  doc["idRemoto"] = r->remoto ? r->recursoRemoto->idRemoto : r->id;

  switch (r->tipo)
  {
  case RECURSO_RELE:
    doc["device"] = releGetJSONDoc(r, true);
    break;

  case RECURSO_SENSOR:
    doc["device"] = sensorGetJSONDoc(r, true);
    break;

  case RECURSO_BOTAO:
    doc["device"] = botaoGetJSONDoc(r, true);
    break;

  case RECURSO_UMIDIFICADOR:
    doc["device"] = umidificadorGetJSONDoc(r, true);
    break;

  default:
    doc["device"] = "???";
    break;
  }

  return doc;
}

// REQUIRE recursosMutex locked
JsonDocument recursoGetJSONEvento(Recurso *r, TipoEvento tipoEvento)
{
  JsonDocument doc;

  doc["origem"] = eTomadaDeviceID();
  doc["id"] = String(r->id);
  doc["evento"] = eventoGetTipoTxt(tipoEvento);

  JsonDocument device;
  switch (r->tipo)
  {
  case RECURSO_RELE:
    device["estado"] = recursoGetRele(r)->estado;
    break;
  case RECURSO_SENSOR:
    device["valor"] = recursoGetSensor(r)->valor;
    break;
  case RECURSO_BOTAO:
    device["estado"] = recursoGetBotao(r)->estado;
    break;
  case RECURSO_UMIDIFICADOR:
    device["estado"] = recursoGetUmidificador(r)->estado;
    device["estadoFan"] = recursoGetUmidificador(r)->estadoFan;
    break;
  }

  doc["device"] = device;

  return doc;
}

String recursoEventoRecebido(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("recursoEventoRecebido", doc, json))
    return "JSON Invalido";

  MutexLock lock(recursosMutex);
  if (!lock)
    return "Erro de LOCK!";

  NodoRemoto *nr = nodoRemotoGet(doc["origem"].as<const char *>());
  if (!nr)
  {
    doc.clear();
    return "Nodo Invalido!";
  }

  int tot = recursosGetCount();
  for (int r = 0; r < tot; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (!rec->remoto)
      continue;
    if (rec->recursoRemoto->nodo != nr) // TODO :: Melhor testar por ID?
      continue;

    if (strcmp(doc["id"].as<const char *>(), rec->recursoRemoto->idRemoto))
      continue;

    logaM(LOG_DEBUG0, "Evento recebido! Atualizar recurso [%s]", rec->id);
    String ret = recursoAtualizaFromJsonLocked(rec, doc["device"], true);

    doc.clear();
    return ret;
  }

  doc.clear();
  return "Recurso nao encontrado";
}

String recursoAtualizaFromJsonLocked(Recurso *recurso, JsonDocument doc, bool enviaEventos)
{
  switch (recurso->tipo)
  {
  case RECURSO_RELE:
  {
    Rele *rele = recursoGetRele(recurso);
    bool novoEstado = doc["estado"].as<bool>();
    bool mudou = (rele->estado != novoEstado);
    rele->estado = novoEstado;
    if (enviaEventos && mudou)
      eventoPost(EVENTO_VALOR_MUDOU, recurso->id, true, true);
  }
  break;

  case RECURSO_SENSOR:
  {
    Sensor *sensor = recursoGetSensor(recurso);

    if (!doc["tipo"].isNull())
      strlcpy(sensor->tipo, doc["tipo"].as<const char *>(), sizeof(sensor->tipo));
    if (!doc["categoria"].isNull())
      strlcpy(sensor->categoria, doc["categoria"].as<const char *>(), sizeof(sensor->categoria));
    if (!doc["unidade"].isNull())
      strlcpy(sensor->unidade, doc["unidade"].as<const char *>(), sizeof(sensor->unidade));
    if (!doc["status"].isNull())
      strlcpy(sensor->status, doc["status"].as<const char *>(), sizeof(sensor->status));

    int novoValor = doc["valor"].as<int>();
    bool mudou = (sensor->valor != novoValor);
    sensor->valor = novoValor;
    if (enviaEventos && mudou)
      eventoPost(EVENTO_VALOR_MUDOU, recurso->id, true, true);
  }
  break;

  case RECURSO_BOTAO:
  {
    Botao *botao = recursoGetBotao(recurso);
    bool novoEstado = doc["estado"].as<bool>();
    bool mudou = (botao->estado != novoEstado);
    botao->estado = novoEstado;
    if (enviaEventos && mudou)
    {
      eventoPost(botao->estado ? EVENTO_LIGOU : EVENTO_DESLIGOU, recurso->id, true, true);
      eventoPost(EVENTO_TOGGLE, recurso->id, true, true);
    }
  }
  break;

  case RECURSO_UMIDIFICADOR:
  {
    Umidificador *umid = recursoGetUmidificador(recurso);
    int novoEstado = doc["estado"].as<int>();
    int novoEstadoFan = doc["estadoFan"].as<int>();

    bool mudou = false;

    if (novoEstado >= UMID_DESLIGADO && novoEstado <= UMID_POWER5)
    {
      if (umid->estado != novoEstado)
        mudou = true;
      umid->estado = (UmidificadorEstado)novoEstado;
    }

    if (novoEstadoFan >= UMIDFAN_DESLIGADO && novoEstadoFan <= UMIDFAN_POWER3)
    {
      if (umid->estadoFan != novoEstadoFan)
        mudou = true;
      umid->estadoFan = (UmidificadorFanEstado)novoEstadoFan;
    }

    if (enviaEventos && mudou)
      eventoPost(EVENTO_VALOR_MUDOU, recurso->id, true, true);
  }
  break;
  }

  return "OK";
}

String recursoAtualizaConfigFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("recursoAtualizaConfigFromJSON", doc, json))
    return "JSON Invalido";

  MutexLock lock(recursosMutex);
  if (!lock)
  {
    doc.clear();
    return "mutex timeout";
  }

  String id = doc["id"];
  Recurso *recurso = recursoGet(id.c_str());
  if (!recurso)
  {
    doc.clear();
    return "Recurso Invalido";
  }

  bool mudou = false;

  if (!doc["nome"].isNull())
  {
    mudou = true;
    String nome = doc["nome"];
    strlcpy(recurso->nome, nome.c_str(), sizeof(recurso->nome));

    // Salvar no Preferences
    Preferences prefs;
    prefs.begin("recursos", false);

    String chave = String("nome") + id;
    prefs.putString(chave.c_str(), nome);

    prefs.end();
  }

  doc.clear();

  if (mudou)
    eventoPost(EVENTO_VALOR_MUDOU, recurso->id, true, true);

  return "OK";
}

int recursoGetValor(Recurso *r)
{
  switch (r->tipo)
  {
  case RECURSO_RELE:
  {
    Rele *rele = recursoGetRele(r);
    return rele->estado;
  }

  case RECURSO_SENSOR:
  {
    Sensor *sensor = recursoGetSensor(r);
    return sensor->valor;
  }

  case RECURSO_BOTAO:
  {
    Botao *botao = recursoGetBotao(r);
    return botao->estado;
  }

  case RECURSO_UMIDIFICADOR:
  {
    Umidificador *umid = recursoGetUmidificador(r);
    return umid->estado;
    // TODO :: e estadoFan?
  }

  default:
    return -999;
  }
}

void recursoPrint(Recurso *recurso)
{
  logaM(LOG_NORMAL, "Recurso%s %s: %s [%s]",
        recurso->remoto ? " Remoto" : "", recurso->id,
        recursoGetTipoStr(recurso->tipo), recurso->nome);
}
