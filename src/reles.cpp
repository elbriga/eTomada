#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "mutex.h"
#include "display.h"
#include "http.h"
#include "prefs.h"

static Rele reles[MAX_RELES];

void relesInit() {
  // Zerar tudo
  memset(reles, 0, sizeof(reles));
  
  Preferences prefs;
  prefs.begin("reles", false);

  // Para testes
  // prefs.putString("nome1", "Luz");
  // prefs.putString("regra1", "OF|02:00|07:59");
  // prefs.putString("pino1", "16");
  // prefs.putString("ativo1", "1");
  
  int totReles = relesGetCount();
  for (int r=1; r <= totReles; r++) {
    Rele *rele = releLoadFromPrefs(r, prefs);

    logaMensagem("Rele %d:%d:%s (%s) > [%s]",
      r, rele->pino, rele->nome,
      (rele->ativo ? "on" : "off"), rele->regra);
  }

  prefs.end();
}

int relesGetCount()
{
  return MAX_RELES;
}

Rele *releGet(int numRele)
{
  if (numRele < 1 || numRele > MAX_RELES) {
    return NULL;
  }

  return &reles[numRele - 1];
}

Rele *releLoadFromPrefs(int num, Preferences &prefs) {
  Rele *rele = releGet(num);
  if (!rele) {
    logaMensagem("ERRO no rele [%d]", num);
    return NULL;
  }

  rele->num = num;
  strncpy(rele->nome,  getPrefsAtr(prefs, num, "nome").c_str(),  sizeof(rele->nome) - 1);
  rele->nome[sizeof(rele->nome) - 1] = '\0';

  rele->pino = atoi(getPrefsAtr(prefs, num, "pino").c_str());

  strncpy(rele->regra, getPrefsAtr(prefs, num, "regra").c_str(), sizeof(rele->regra) - 1);
  rele->regra[sizeof(rele->regra) - 1] = '\0';

  String regraOK = validaRegra(rele->regra);
  if (regraOK != "OK") {
    logaMensagem("Regra [%s] INVALIDA! [%s] Desativando Rele[%d]", rele->regra, regraOK.c_str(), num);
  }
  bool pinoOK = eTomadaPinoOutOK(rele->pino);
  if (!pinoOK && rele->pino != -1) {
    logaMensagem("Pino [%d] INVALIDO! Desativando Rele[%d]", rele->pino, num);
  }
  rele->ativo = (regraOK == "OK" && pinoOK) ?
    (getPrefsAtr(prefs, num, "ativo") == "1") : false;

  // TODO :: guardar estado dos reles ativos e sem regra (modo manual) para voltar ao estado certo no boot
  rele->estado = 0;
  rele->override = 0;

  return rele;
}

String relesSetFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }
 
  int numRele = doc["rele"];
  if (numRele < 1 || numRele > MAX_RELES) {
    return "Rele invalido";
  }

  bool estado = (doc["estado"].as<String>() == "1");

  String msg = releControla(numRele, estado, 30 * 60); // TODO tirar o hardcoded de 30 minutos
  if (msg != "") {
    // logaMensagem(msg.c_str());
    displayMostraMsg(msg.c_str(), 5000);
  }

  return "OK";
}

// REQUIRE releMutex locked
JsonDocument releGetJSONDoc(Rele *r) {
  JsonDocument doc;

  doc["num"]      = r->num;
  doc["pino"]     = r->pino;
  doc["nome"]     = r->nome;
  doc["regra"]    = r->regra;
  doc["ativo"]    = r->ativo;
  doc["estado"]   = r->estado;
  doc["override"] = r->override;

  return doc;
}

// REQUIRE releMutex locked
String releGetJSONString(Rele *r) {
  String out;
  JsonDocument doc = releGetJSONDoc(r);

  serializeJson(doc, out);
  return out;
}

String releAtualizaConfigFromJSON(uint8_t *json)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }

  int numRele = doc["rele"];
  if (numRele < 1 || numRele > MAX_RELES) {
    return "Rele invalido";
  }

  Rele releCopy;
  {
    MutexLock lock(releMutex, pdMS_TO_TICKS(2500));
    if (!lock) {
      return "mutex timeout";
    }

    Rele *rele = &reles[numRele - 1];

    String novaRegra = doc["regra"].isNull() ? String(rele->regra) : doc["regra"].as<String>();
    String regraOK = validaRegra(novaRegra);
    if (regraOK != "OK") {
      return "Regra Invalida :: " + regraOK;
    }
    strncpy(rele->regra, novaRegra.c_str(), sizeof(rele->regra) - 1);
    rele->regra[sizeof(rele->regra) - 1] = '\0';

    int novoPino = doc["pino"].isNull() ? rele->pino :atoi(doc["pino"].as<String>().c_str());
    if (novoPino != -1 && !eTomadaPinoOutOK(novoPino)) {
      return "Pino Invalido";
    }
    rele->pino = novoPino;

    if (!doc["nome"].isNull()) {
      strncpy(rele->nome, doc["nome"].as<String>().c_str(), sizeof(rele->nome) - 1);
      rele->nome[sizeof(rele->nome) - 1] = '\0';
    }

    if (!doc["ativo"].isNull()) {
      rele->ativo = (doc["ativo"] == "1" || doc["ativo"] == 1);
    }

    memcpy(&releCopy, rele, sizeof(Rele));
  }
  
  logaMensagem(">> RELE [%d] nome[%s] regra[%s] pino[%d] ativo[%d]",
    numRele, releCopy.nome, releCopy.regra, releCopy.pino, releCopy.ativo);
  
  // Setar no prefs
  eTomadaSalvaRele(&releCopy);

  String releJSON = releGetJSONString(&releCopy);
  httpEnviaEvento(releJSON, "sse_rele");

  return "OK";
}

String releControla(int numRele, bool estado, int override)
{
  MutexLock lock(releMutex);
  if (!lock) {
    return "releControla: mutex timeout";
  }

  return releControlaUnsafe(numRele, estado, override);
}

// REQUIRE releMutex locked
String releControlaUnsafe(int numRele, bool estado, int override)
{
  Rele *rele = releGet(numRele);
  if (!rele) {
    logaMensagem("controlaRele: numRele [%d] invalido!\n", numRele);
    return "";
  }
  
  if (rele->pino == -1) {
    logaMensagem("controlaRele[%d]: pino invalido!\n", numRele);
    return "";
  }

  String ret = "";
  if (estado != rele->estado) {
    digitalWrite(rele->pino, estado);
    rele->estado = estado;

    rele->override = (override > 0) ? time(nullptr) + override : 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "%s %s (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"),
      rele->nome, numRele, rele->pino);
    ret = msg;

    String releJSON = releGetJSONString(rele);
    httpEnviaEvento(releJSON, "sse_rele");
  }

  return ret;
}
