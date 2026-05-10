#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "reles.h"
#include "regras.h"
#include "ntp.h"

// Valores default
static Rele relesConfigDefault[MAX_RELES] = {
  { 1, 15, "Luz",             "OF>02:00-07:59", 1, 0 },
  { 2, 13, "Umidificador",    "ON>08:00-20:00", 1, 0 },
  { 3, 12, "Ventilador",      "OF>01:00-02:00", 1, 0 },
  { 4, 14, "Desumidificador", "",               1, 0 },
  { 5, -1, "", "", 0, 0 },
  { 6, -1, "", "", 0, 0 },
  { 7, -1, "", "", 0, 0 },
  { 8, -1, "", "", 0, 0 }
};

// Salvar as regras na memoria FLASH
Preferences prefs;
String getPrefsAtr(int num, String nomeAtr);
String setPrefsAtr(int num, String nomeAtr, String val);

void eTomadaLoadConfig() {
  Serial.println("Carregando Configuracao dos reles:");

  prefs.begin("reles", false);

  // Para testes
  // prefs.putString("nome1", "Luz");
  // prefs.putString("regra1", "OF-02:00-07:59");
  // prefs.putString("pino1", "15");

  Rele *rele;
  int totReles = relesGetCount();
  for (int r=1; r <= totReles; r++) {
    rele = releGet(r);
    if (!rele) continue;

    rele->num   = r;
    strncpy(rele->nome,  getPrefsAtr(r, "nome").c_str(),  sizeof(rele->nome) - 1);
    strncpy(rele->regra, getPrefsAtr(r, "regra").c_str(), sizeof(rele->regra) - 1);
    rele->ativo = getPrefsAtr(r, "ativo") == "1";
    rele->pino  = atoi(getPrefsAtr(r, "pino").c_str());

    Serial.printf("Rele %d:%d (%s) > [%s]\n", r, rele->pino, rele->nome, rele->regra);
  }

  Serial.println("");
}

String eTomadaGetDataJSON() {
  JsonDocument doc;

  time_t agora;
  struct tm timeinfo;
  ntpGetTime(&timeinfo, &agora);
  
  doc["datahora"] = (unsigned long)agora;
  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  doc["datahorastr"] = formattedTime;

  Rele *rele;
  int totReles = relesGetCount();
  JsonArray arr = doc["reles"].to<JsonArray>();
  for (int i = 1; i <= totReles; i++) {
    rele = releGet(i);
    if (!rele) continue;

    JsonObject r = arr.add<JsonObject>();
    r["num"]    = rele->num;
    r["nome"]   = rele->nome;
    r["regra"]  = rele->regra;
    r["pino"]   = rele->pino;
    r["estado"] = rele->estado;
    r["ativo"]  = rele->ativo;
  }

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaSalvaRele(Rele *rele) {
  setPrefsAtr(rele->num, "nome",  String(rele->nome));
  setPrefsAtr(rele->num, "regra", String(rele->regra));
  setPrefsAtr(rele->num, "ativo", String(rele->ativo));

  int oldPin = atoi(setPrefsAtr(rele->num, "pino",  String(rele->pino)).c_str());
  if (rele->pino != oldPin) {
    // Desligar pino antigo
    if (oldPin != -1) {
      digitalWrite(oldPin, LOW);
    }

    // Ativar o pino novo
    if (rele->pino != -1) {
      pinMode(rele->pino, OUTPUT);
    }
  }
}

void eTomadaFactoryReset() {
  int totReles = relesGetCount();

  prefs.clear();
  // TODO :: prefs.putString("totReles", String(totReles));

  Rele *rele;
  for (int r=1; r <= totReles; r++) {
    rele = releGet(r);
    memcpy(rele, &relesConfigDefault[r - 1], sizeof(Rele));
    eTomadaSalvaRele(rele);
  }

  processaRegras();
}

String getPrefsAtr(int num, String nomeAtr) {
  char buff[10];
  sprintf(buff, "%s%d", nomeAtr.c_str(), num);
  return prefs.isKey(buff) ? prefs.getString(buff, "") : "";
}

String setPrefsAtr(int num, String nomeAtr, String val) {
  String old = getPrefsAtr(num, nomeAtr);

  if (val != old) {
    char buff[10];
    sprintf(buff, "%s%d", nomeAtr.c_str(), num);
    prefs.putString(buff, val);
  }

  return old;
}
