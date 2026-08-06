#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "eTomada.h"
#include "mestre.h"
#include "rele.h"
#include "regras.h"
#include "tipoSensores.h"
#include "sensor.h"
#include "ntp.h"
#include "mutex.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "recurso.h"
#include "discover.h"
#include "anunciador.h"

// Modo de Operação
ModoOperacao modoOperacao = MODO_NO;

void eTomadaInit()
{
  mutexInit();

  Preferences prefs;
  prefs.begin("eTomada", false);

  if (!prefs.isKey("modo"))
    prefs.putUChar("modo", MODO_NO);

  modoOperacao =
      (prefs.getUChar("modo") == MODO_CONTROLADOR) ? MODO_CONTROLADOR : MODO_NO;

  logaMensagem("Modo de Operação: %s", eTomadaGetModoOperacaoStr());
  logaMensagem("MAC: %s", getMACStr().c_str());

  mestreInit(prefs);

  prefs.end();

  anunciadorInit();

  logaMensagem("Inicializando Relés Locais:");
  relesInit();

  logaMensagem("Inicializando Sensores Locais:");
  sensoresInit();

  logaMensagem("Inicializando Botões Locais:");
  botoesInit();

  logaMensagem("Inicializando o Discover:");
  discoverInit();

  logaMensagem("Carregando Nodos Remotos:");
  nodoRemotoInit();

  logaMensagem("Inicializando Recursos Remotos:");
  recursosRemotosInit();

  logaMensagem("Inicializando Recursos:");
  recursosInit();

  Serial.println("");
}

ModoOperacao eTomadaGetModoOperacao()
{
  return modoOperacao;
}

const char *eTomadaGetModoOperacaoStr()
{
  switch (modoOperacao)
  {
  case MODO_CONTROLADOR:
    return "CONTROLADOR";
  case MODO_NO:
    return "NÓ";
  default:
    return "MODOOPERACAOINVALIDO!";
  }
}

String eTomadaGetSnapshotJSON()
{
  JsonDocument doc;

  uint64_t MAC = getMAC();

  char deviceID[32];
  sprintf(deviceID, "etomada_%04X", (uint16_t)(MAC & 0xFFFF));
  doc["device_id"] = deviceID;
  doc["device_name"] = "eTomada Sala"; // TODO
  doc["fw_version"] = "1.3.0";

  doc["mac"] = getMACStr();

  doc["api"] = 3; // versão da API

  doc["uptime"] = millis();
  time_t now = 0;
  time(&now);
  doc["timestamp"] = (unsigned long)now;

  time_t agora;
  struct tm timeinfo;
  ntpGetTime(&timeinfo, &agora);

  doc["datahora"] = (unsigned long)agora;
  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  doc["datahorastr"] = formattedTime;

  // TODO :: Enviar estes dados no RECURSO_SENSOR, mesmo que duplicados, assim o sensor fica auto-suficiente e pode ser mostrado no nodo pai
  {
    TipoSensor *ts;
    int totTS = tipoSensorGetCount();
    JsonArray tipoSensores = doc["tipoSensores"].to<JsonArray>();
    for (int i = 0; i < totTS; i++)
    {
      ts = tipoSensorGetPorIndice(i);
      if (!ts)
        continue;

      tipoSensores.add(tipoSensorGetJSONDoc(ts));
    }
  }

  Recurso *recurso;
  int totRecursos = recursosGetCount();
  JsonArray recursos = doc["recursos"].to<JsonArray>();
  for (int i = 0; i < totRecursos; i++)
  {
    recurso = recursoGetPorIndice(i);
    if (!recurso)
      continue;

    recursos.add(recursoGetJSONDoc(recurso));
  }

  String out;
  serializeJson(doc, out);

  return out;
}

void eTomadaSalvaRele(Recurso *recurso)
{
  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock)
  {
    logaMensagem("eTomadaSalvaRele: erro de mutex!");
    return;
  }

  Preferences prefs;
  if (recurso->remoto)
    prefs.begin("recursosRemotos", false);
  else
    prefs.begin("reles", false);

  int num = recursoGetNum(recurso);
  Rele *rele = recursoGetRele(recurso);
  setPrefsAtr(prefs, num, "nome", String(rele->nome));
  setPrefsAtr(prefs, num, "regra", String(rele->regra));
  setPrefsAtr(prefs, num, "ativo", String(rele->ativo));

  prefs.end();
}

void eTomadaSalvaSensor(Recurso *recurso)
{
  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock)
  {
    logaMensagem("eTomadaSalvaSensor: erro de mutex!");
    return;
  }

  Preferences prefs;
  if (recurso->remoto)
    prefs.begin("recursosRemotos", false);
  else
    prefs.begin("sensores", false);

  int num = recursoGetNum(recurso);
  Sensor *sensor = recursoGetSensor(recurso);
  setPrefsAtr(prefs, num, "nome", String(sensor->nome));
  setPrefsAtr(prefs, num, "pino", String(sensor->pino));
  setPrefsAtr(prefs, num, "tipo", String(sensor->tipo));

  prefs.end();
}

void eTomadaSalvaBotao(Recurso *recurso)
{
  if (recurso->tipo != RECURSO_BOTAO)
  {
    logaMensagem("eTomadaSalvaBotao: erro de tipo de recurso!");
    return;
  }

  MutexLock lock(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lock)
  {
    logaMensagem("eTomadaSalvaBotao: erro de mutex!");
    return;
  }

  Preferences prefs;
  if (recurso->remoto)
    prefs.begin("recursosRemotos", false);
  else
    prefs.begin("botoes", false);

  int num = recursoGetNum(recurso);
  Botao *botao = recursoGetBotao(recurso);
  // TODO setPrefsAtr(prefs, num, "nome", String(botao->nome));
  setPrefsAtr(prefs, num, "ativo", String(botao->ativo));

  prefs.end();
}

void eTomadaRoleta()
{
  logaTitulo("ROLETA!");

  int totRelesLocais = 0;
  int totRecursos = recursosGetCount();
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *recurso = recursoGetPorIndice(r);
    if (recurso->tipo == RECURSO_RELE && !recurso->remoto)
    {
      totRelesLocais++;
    }
  }

  Recurso **relesLocais = (Recurso **)calloc(sizeof(Recurso *), totRelesLocais);
  if (!relesLocais)
  {
    logaTitulo("ROLETA :: ERRO DE MALLOC");
    return;
  }

  int rli = 0;
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *recurso = recursoGetPorIndice(r);
    if (recurso->tipo == RECURSO_RELE && !recurso->remoto)
    {
      relesLocais[rli++] = recurso;
    }
  }

  for (int r = 0; r < totRelesLocais; r++)
  {
    Recurso *recurso = relesLocais[r];
    recursoSet(recurso, false);
  }

  int delay = 25, delta = 2;
  int num = esp_random() % totRelesLocais;
  int oldNum = num;
  int loop = 0;

  while (delay < 440)
  {
    esp_task_wdt_reset(); // alimenta o watchdog

    oldNum = num;
    num++;
    if (num >= totRelesLocais)
    {
      num = 0;
    }
    recursoSet(relesLocais[oldNum], false);
    recursoSet(relesLocais[num], true);

    loop++;
    if (loop > 40)
    {
      delay += delta;
      if (loop > 90)
      {
        delta += 1;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(delay));
  }

  free(relesLocais);

  logaMensagem("** Numero Sorteado: %d **", num + 1);
}

void eTomadaFactoryReset()
{
  MutexLock lockPrefs(prefsMutex, pdMS_TO_TICKS(2500));
  if (!lockPrefs)
  {
    logaMensagem("Erro de mutex no factory reset!");
    return;
  }

  Preferences prefs;

  prefs.begin("reles", false);
  prefs.clear();
  prefs.end();

  prefs.begin("sensores", false);
  prefs.clear();
  prefs.end();

  prefs.begin("recursosRemotos", false);
  prefs.clear();
  prefs.end();

  logaTitulo("RESET!");
  ESP.restart();
}

uint64_t getMAC()
{
  return ESP.getEfuseMac();
}

String getMACStr()
{
  uint64_t MAC = getMAC();

  char macStr[18];
  sprintf(
      macStr,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      (uint8_t)(MAC >> 40),
      (uint8_t)(MAC >> 32),
      (uint8_t)(MAC >> 24),
      (uint8_t)(MAC >> 16),
      (uint8_t)(MAC >> 8),
      (uint8_t)(MAC));

  return String(macStr);
}
