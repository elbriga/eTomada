#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "http.h"
#include "eventos.h"
#include "mutex.h"
#include "agendamentos.h"
#include "util.h"
#include "umidificador.h"
#include "led.h"
#include "mic.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RECURSO", nivel, fmt, ##__VA_ARGS__)

static Recurso *recursos;
static int totRecursos = 0;
int recursoAddCount = 0;

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, const char *id);

void recursosInit()
{
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();
  int totBotoesLocais = botoesGetCount();

  totRecursos = totRelesLocais + totSensoresLocais + totBotoesLocais +
                (umidificadorAtivo() ? 1 : 0) + (micAtivo() ? 1 : 0);

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

  if (umidificadorAtivo())
  {
    Recurso *r = recursoAdd(prefs, RECURSO_UMIDIFICADOR, "UMIDIFICADOR");
    r->umid = umidificadorGet();
  }

  if (micAtivo())
  {
    Recurso *r = recursoAdd(prefs, RECURSO_MICROFONE, "MICROFONE");
    r->mic = micGet();
  }

  prefs.end();

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

Recurso *recursoAdd(Preferences &prefs, TipoRecurso tipo, const char *id)
{
  if (recursoAddCount >= totRecursos)
    utilDIE("recursoAdd!! DIE!!");

  Recurso *r = &recursos[recursoAddCount++];

  r->tipo = tipo;

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

bool recursoSetFromJSON(uint8_t *json, String &recursoIDOut, String &msgOut, bool enviaEventos)
{
  recursoIDOut = "";

  JsonDocument jsonIN;
  if (utilLeJson("recursoSetFromJSON", jsonIN, json))
  {
    msgOut = "JSON Invalido";
    return false;
  }

  String id = jsonIN["id"].as<String>();
  String estado = jsonIN["estado"].as<String>();
  String estadoFan = jsonIN["estadoFan"].as<String>();
  jsonIN.clear();

  Recurso *recurso = recursoGet(id.c_str());
  if (!recurso)
  {
    msgOut = "recursoSetFromJson : Recurso invalidooo!";
    return false;
  }

  recursoIDOut = id;

  if (estadoFan != "" && estadoFan != "null")
    estado += ":" + estadoFan;
  return recursoSet(id.c_str(), estado, msgOut, enviaEventos);
}

bool recursoSetLocalLocked(Recurso *recurso, String estado, String &msgOut)
{
  if (recurso->tipo != RECURSO_RELE && recurso->tipo != RECURSO_UMIDIFICADOR)
  {
    msgOut = "Erro recursoSetLocalLocked: Recurso nao eh RELE nem UMID";
    return false;
  }

  bool ret;
  {
    switch (recurso->tipo)
    {
    case RECURSO_RELE:
    {
      ret = releControla(recurso, estado == "ON", msgOut);
    }
    break;

    case RECURSO_UMIDIFICADOR:
    {
      UmidificadorEstado umidEstado = (UmidificadorEstado)estado.toInt();
      if (umidEstado < UMID_DESLIGADO || umidEstado > UMID_POWER5)
      {
        msgOut = "recursoSetLocalLocked: Estado UMID invalido";
        return false;
      }

      int temEstadoFan = estado.indexOf(':');
      if (temEstadoFan >= 0)
      {
        UmidificadorFanEstado estadoFan = (UmidificadorFanEstado)estado.substring(temEstadoFan + 1).toInt();
        umidificadorFanSetEstado(estadoFan);
      }
      ret = umidificadorSetEstado(umidEstado, msgOut);
    }
    break;
    }
  }

  return ret;
}

bool recursoSet(const char *recursoID, String estado, String &msgOut, bool enviaEventos)
{
  // Recurso virtual __ALERTA__
  if (!strcmp(recursoID, "__ALERTA__"))
  {
    ledSetAnim(estado == "ON" ? LED_ANIM_ALARM : LED_ANIM_OFF);
    return true;
  }

  bool ret = false;
  bool enviaToggle = false;
  String estadoFinal = estado;

  {
    MutexLock lock(modeloMutex, "recursoSet");
    if (!lock)
    {
      msgOut = "recursoSet: mutex timeout";
      return false;
    }

    Recurso *recurso = recursoGet(recursoID);
    if (!recurso)
    {
      msgOut = "recursoSet: recurso invalido";
      return false;
    }

    if (recurso->tipo != RECURSO_RELE && recurso->tipo != RECURSO_UMIDIFICADOR)
    {
      msgOut = "recursoSet: Recurso nao eh RELE nem UMID";
      return false;
    }

    if (recurso->tipo == RECURSO_RELE)
    {
      enviaToggle = true;

      if (estado == "TOGGLE")
      {
        Rele *r = recurso->rele;
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

    // Recursos locais: Tratar dentro do Lock
    ret = recursoSetLocalLocked(recurso, estadoFinal, msgOut);
  }

  if (ret)
  {
    // Limpar os agendamentos do recurso, caso existam
    agendamentosLimpa(recursoID);

    if (estado == "PULSE")
    {
      // Agendar o OFF = pulso de 1000ms
      agendamentosAdd(AGEND_RECURSO, 1000, recursoID, "OFF");
    }

    if (enviaEventos)
    {
      eventoPost(EVENTO_VALOR_MUDOU, recursoID, true, true);
      if (enviaToggle)
        eventoPost(EVENTO_TOGGLE, recursoID, false, false); // RELES: Nao envia atualizacoes, mas serve para as regras
    }
  }

  return ret;
}

void recursoEnviaSSE(const char *recursoID)
{
  String recursoStr;
  {
    MutexLock lock(modeloMutex, "recursoEnviaSSE");
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
  case RECURSO_MICROFONE:
    return "MICROFONE";
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
  case RECURSO_MICROFONE:
    return 'M';
  default:
    return '0';
  }
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
  if (tipoStr == "MICROFONE")
    return RECURSO_MICROFONE;

  return RECURSO_INVALIDO;
}

// TODO Unificar o metodo abaixo somente com este
JsonDocument recursoGetJSONDoc(Recurso *r)
{
  JsonDocument doc;

  doc["id"] = r->id;
  doc["tipo"] = recursoGetTipoStr(r->tipo);
  doc["nome"] = r->nome;

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

  case RECURSO_MICROFONE:
    doc["device"] = micGetJSONDoc(r, true);
    break;

  default:
    doc["device"] = "???";
    break;
  }

  return doc;
}

String recursoGetJSONEvento(const char *recursoID, TipoEvento tipoEvento)
{
  String ret = "{}";
  {
    MutexLock lock(modeloMutex, "recursoGetJSONEvento");
    if (!lock)
      return ret;

    Recurso *r = recursoGet(recursoID);
    if (!r)
      return ret;

    JsonDocument doc;

    doc["origem"] = eTomadaDeviceID();
    doc["id"] = String(r->id);
    doc["evento"] = eventoGetTipoTxt(tipoEvento);

    JsonDocument device;
    switch (r->tipo)
    {
    case RECURSO_RELE:
      device["estado"] = r->rele->estado;
      break;
    case RECURSO_SENSOR:
    {
      Sensor *s = r->sensor;
      device["valor"] = s->valor;
      device["status"] = s->status;
    }
    break;
    case RECURSO_BOTAO:
      device["estado"] = r->botao->estado;
      break;
    case RECURSO_UMIDIFICADOR:
      device["estado"] = r->umid->estado;
      device["estadoFan"] = r->umid->estadoFan;
      break;
    case RECURSO_MICROFONE:
      device["ativou"] = "TODO!";
      break;
    }

    doc["device"] = device;

    serializeJson(doc, ret);
  }

  return ret;
}

String recursoAtualizaConfigFromJSON(uint8_t *json)
{
  JsonDocument doc;
  if (utilLeJson("recursoAtualizaConfigFromJSON", doc, json))
    return "JSON Invalido";

  MutexLock lock(modeloMutex, "recursoAtualizaConfigFromJSON");
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
    eventoPost(EVENTO_VALOR_MUDOU, recurso->id, true, false); // Nao enviar para o server alteração de config

  return "OK";
}

int recursoGetValor(Recurso *r)
{
  if (!r)
    return 0;

  switch (r->tipo)
  {
  case RECURSO_RELE:
    return r->rele->estado;

  case RECURSO_SENSOR:
    return r->sensor->valor;

  case RECURSO_BOTAO:
    return r->botao->estado;

  case RECURSO_UMIDIFICADOR:
    return r->umid->estado;

  case RECURSO_MICROFONE:
    return 0; // TODO?

  default:
    return -999;
  }
}

void recursoPrint(Recurso *recurso)
{
  logaM(LOG_NORMAL, "Recurso %s: %s [%s]",
        recurso->id, recursoGetTipoStr(recurso->tipo), recurso->nome);
}
