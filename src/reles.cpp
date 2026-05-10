#include <Arduino.h>
#include <ArduinoJson.h>

#include "reles.h"
#include "regras.h"
#include "eTomada.h"

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

  Rele *rele = &reles[numRele - 1];

  String novaRegra = doc["regra"].isNull() ? rele->regra : doc["regra"];
  String regraOK = validaRegra(novaRegra);
  if (regraOK != "OK") {
    return "Regra Invalida :: " + regraOK;
  }

  rele->regra = novaRegra;
  rele->nome  = doc["nome"].isNull()  ? rele->nome  : doc["nome"];
  rele->pino  = doc["pino"].isNull()  ? rele->pino  : atoi(doc["pino"].as<String>().c_str());
  rele->ativo = doc["ativo"].isNull() ? rele->ativo : (doc["ativo"] == "1" || doc["ativo"] == 1);
  
  Serial.println(">> RELE " + String(numRele) + " nome:" + rele->nome +
    " regra:" + rele->regra + " pino:" + String(rele->pino) + " ativo:" + String(rele->ativo ? "1" : "0"));
  
  // Setar no prefs
  eTomadaSalvaRele(rele);

  return "OK";
}

String releControla(int numRele, bool estado) {
  Rele *rele = releGet(numRele);
  if (!rele) {
    Serial.printf("controlaRele: numRele [%d] invalido!\n", numRele);
    return "";
  }
  
  if (rele->pino == -1) {
    Serial.printf("controlaRele[%d]: pino invalido!\n", numRele);
    return "";
  }

  String ret = "";
  if (estado != rele->estado) {
    digitalWrite(rele->pino, estado);
    rele->estado = estado;

    char msg[128];
    sprintf(msg, "%s %s (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"), rele->nome.c_str(), numRele, rele->pino);
    ret = msg;
  }

  return ret;
}
