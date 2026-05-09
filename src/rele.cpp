#include <Arduino.h>
#include <ArduinoJson.h>

#include "rele.h"
#include "regra.h"
#include "eTomada.h"

String releAtualizaConfigFromJSON(uint8_t *json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    return "JSON Invalido";
  }

  int numRele = doc["rele"];
  if (numRele < 1 || numRele > 8) {
    return "Rele invalido";
  }

  String novaRegra = doc["regra"];
  if (!novaRegra) {
    return "Regra Zerada";
  }

  String regraOK = validaRegra(novaRegra);
  if (regraOK != "") {
    return "Regra Invalida";
  }

  Rele *rele = &reles[numRele - 1];
  String nome  = rele->nome;
  String regra = rele->regra;
  int    pino  = rele->pino;
  rele->nome  = doc["nome"]  | rele->nome;
  rele->regra = doc["regra"] | rele->regra;
  rele->pino  = doc["pino"]  | rele->pino;
  
  Serial.println(">> RELE " + String(numRele) + " nome:" + rele->nome +
    " regra:" + rele->regra + " pino:" + String(rele->pino));
  
  // Setar no prefs
  eTomadaSalvaRele(numRele, rele);

  return "OK";
}

String releControla(int numRele, bool estado) {
  if (numRele < 1 || numRele > 8) {
    Serial.printf("controlaRele: numRele [%d] invalido!\n", numRele);
    return "";
  }
  
  Rele *rele = &reles[numRele - 1];
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

