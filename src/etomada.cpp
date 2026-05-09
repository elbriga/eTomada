#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "rele.h"

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

  for (int r=0; r < 8; r++) {
    Rele *rele = &reles[r];

    rele->nome  = getPrefsAtr(r+1, "nome");
    rele->regra = getPrefsAtr(r+1, "regra");
    rele->pino  = atoi(getPrefsAtr(r+1, "pino").c_str());

    Serial.printf("Rele %d:%d (%s) > [%s]\n", r+1, rele->pino, rele->nome.c_str(), rele->regra.c_str());
  }

  Serial.println("");
}

String eTomadaGetDataJSON() {
  JsonDocument doc;

  time_t agora = time(nullptr);
  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);
  doc["datahora"] = (unsigned long)agora;
  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  doc["datahorastr"] = formattedTime;

  JsonArray arr = doc["reles"].to<JsonArray>();
  for (int i = 0; i < 8; i++) {
      JsonObject r = arr.add<JsonObject>();
      Rele *rele = &reles[i];
      r["nome"]   = rele->nome;
      r["regra"]  = rele->regra;
      r["pino"]   = rele->pino;
      r["estado"] = rele->estado;
  }

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaSalvaRele(int numRele, Rele *rele) { // TODO Rele->num
  setPrefsAtr(numRele, "nome",  rele->nome);
  setPrefsAtr(numRele, "regra", rele->regra);

  int oldPin = atoi(setPrefsAtr(numRele, "pino",  String(rele->pino)).c_str());
  if (rele->pino != oldPin) {
    // Desligar pino antigo
    digitalWrite(oldPin, LOW);
    // Ativar o pino novo
    pinMode(rele->pino, OUTPUT);
  }
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
