#include <Arduino.h>
#include <ArduinoJson.h>

#include "eTomada.h"
#include "loga.h"
#include "reles.h"
#include "regras.h"
#include "mutex.h"

static Rele reles[MAX_RELES];

int relesGetCount() {
  return MAX_RELES;
}

Rele *releGet(int numRele) {
  if (numRele < 1 || numRele > MAX_RELES) {
    return NULL;
  }

  return &reles[numRele - 1];
}

String relesAtualizaConfigFromJSON(uint8_t *json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }

  int numRele = doc["rele"];
  if (numRele < 1 || numRele > MAX_RELES) {
    return "Rele invalido";
  }

  if (!xSemaphoreTake(releMutex, pdMS_TO_TICKS(1000))) {
    return "mutex timeout";
  }

  Rele *rele = &reles[numRele - 1];

  String novaRegra = doc["regra"].isNull() ? String(rele->regra) : doc["regra"].as<String>();
  String regraOK = validaRegra(novaRegra);
  if (regraOK != "OK") {
    xSemaphoreGive(releMutex);
    return "Regra Invalida :: " + regraOK;
  }
  strncpy(rele->regra, novaRegra.c_str(), sizeof(rele->regra) - 1);
  rele->regra[sizeof(rele->regra) - 1] = '\0';

  int novoPino = doc["pino"].isNull() ? rele->pino :atoi(doc["pino"].as<String>().c_str());
  if (novoPino != -1 && !eTomadaPinoOK(novoPino)) {
    xSemaphoreGive(releMutex);
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
  
  logaMensagem(">> RELE [%d] nome[%s] regra[%s] pino[%d] ativo[%d]",
    numRele, rele->nome, rele->regra, rele->pino, rele->ativo);
  
  // Setar no prefs
  eTomadaSalvaRele(rele);

  xSemaphoreGive(releMutex);

  return "OK";
}

String releControla(int numRele, bool estado) {
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

    char msg[128];
    sprintf(msg, "%s %s (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"),
      rele->nome, numRele, rele->pino);
    ret = msg;
  }

  return ret;
}
