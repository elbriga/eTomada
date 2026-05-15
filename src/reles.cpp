#include <Arduino.h>
#include <ArduinoJson.h>

#include "eTomada.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "mutex.h"

static Rele reles[MAX_RELES];

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
    logaMensagem(msg.c_str());
  }

  return "OK";
}

String relesAtualizaConfigFromJSON(uint8_t *json)
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
    if (novoPino != -1 && !eTomadaPinoOK(novoPino)) {
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
  }

  return ret;
}
