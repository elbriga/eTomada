#include <Arduino.h>
#include <esp_task_wdt.h>
#include <stdarg.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "eTomada.h"
#include "loga.h"
#include "ntp.h"
#include "prefs.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("LOGS", nivel, fmt, ##__VA_ARGS__)

#define LOG_QUEUE_SIZE 32
#define LOG_MESSAGE_SIZE 512

#define LOG_SERVER "192.168.1.220:8080"

String logServer = "";
LogLevel logLevel = LOG_NORMAL;

struct LogRemoto
{
  time_t timestamp;
  uint32_t uptime; // em segundos -> TODO :: mudar para ms?
  int level;
  char modulo[16];
  char message[LOG_MESSAGE_SIZE];
};

static QueueHandle_t logQueue = nullptr;

static void logRemotoTask(void *param);
void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args);

String logaGetLogServer()
{
  return logServer;
}

bool logaRemotoAtivo()
{
  return (logServer != "");
}

void logaInit()
{
  Preferences prefs;
  prefs.begin("eTomada", false); // usando o mesmo namespace de eTomada.cpp

  // Para testes
  // prefs.putString("logLevel", String(LOG_DEBUG));
  // prefs.putString("logServer", "192.168.1.220:8080");

  int levelPrefs = getPrefsAtr(prefs, "", "logLevel").toInt();
  switch (levelPrefs)
  {
  case LOG_CRITICO:
    logLevel = LOG_CRITICO;
    break;
  case LOG_AVISO:
    logLevel = LOG_AVISO;
    break;
  case LOG_NORMAL:
    logLevel = LOG_NORMAL;
    break;
  case LOG_DEBUG0:
    logLevel = LOG_DEBUG0;
    break;
  case LOG_DEBUG:
    logLevel = LOG_DEBUG;
    break;
  case LOG_TESTE:
    logLevel = LOG_TESTE;
    break;
  default:
    logLevel = LOG_NORMAL;
    break;
  }

  logServer = LOG_SERVER; // getPrefsAtr(prefs, "", "logServer");

  prefs.end();

  if (!logaRemotoAtivo())
  {
    logaM(LOG_AVISO, "==========================");
    logaM(LOG_AVISO, "Sem logServer configurado!");
    logaM(LOG_AVISO, "==========================");
    return;
  }

  // TODO validar logServer se tem a porta tbm

  if (logQueue)
  {
    logaM(LOG_CRITICO, "ERRO: logaInit() chamado duas vezes?");
    return;
  }
  logQueue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogRemoto));
  if (!logQueue)
  {
    logaM(LOG_CRITICO, "ERRO: nao foi possivel criar fila de logs");
    return;
  }

  xTaskCreate(
      logRemotoTask,
      "logRemoto",
      4096,
      nullptr,
      1,
      nullptr);

  logaM(LOG_NORMAL, "Log remoto inicializado em %s", logServer.c_str());
}

void loga(const char *modulo, LogLevel nivel, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  logaV(modulo, nivel, fmt, args);

  va_end(args);
}

const char *logaGetNivelTxt(LogLevel nivel)
{
  switch (nivel)
  {
  case LOG_DESATIVADO:
    return "!OFF!!";
  case LOG_CRITICO:
    return "!CRIT!";
  case LOG_AVISO:
    return "AVISO!";
  case LOG_NORMAL:
    return "NORMAL";
  case LOG_DEBUG0:
    return "DEBUG0";
  case LOG_DEBUG:
    return "DEBUG!";
  case LOG_TESTE:
    return "TESTE!";
  default:
    return "??????";
  }
}

void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args)
{
  esp_task_wdt_reset(); // alimenta o watchdog

  if (nivel == LOG_DESATIVADO || nivel > logLevel)
    // ignorar
    return;

  // Gerar o log
  char msg[LOG_MESSAGE_SIZE];
  vsnprintf(msg, sizeof(msg), fmt, args);

  // Obter horario
  struct tm timeinfo;
  sysGetTime(&timeinfo);

  char formattedTime[32] = {0};
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);

  char formattedUptime[32] = {0};
  uint32_t uptime = millis() / 1000;
  int dias = uptime / 86400;
  int horas = (uptime % 86400) / 3600;
  int minutos = (uptime % 3600) / 60;
  int segundos = uptime % 60;
  if (dias > 0)
  {
    snprintf(formattedUptime, sizeof(formattedUptime), "%dd %s%d:%s%d:%s%d",
             dias,
             (horas < 10 ? "0" : ""), horas,
             (minutos < 10 ? "0" : ""), minutos,
             (segundos < 10 ? "0" : ""), segundos);
  }
  else
  {
    snprintf(formattedUptime, sizeof(formattedUptime), "%s%d:%s%d:%s%d",
             (horas < 10 ? "0" : ""), horas,
             (minutos < 10 ? "0" : ""), minutos,
             (segundos < 10 ? "0" : ""), segundos);
  }

  char pontosPadModulo[8];
  {
    size_t len = strlen(modulo);
    size_t pad = len < 7 ? 7 - len : 0;
    for (int i = 0; i < pad; i++)
      pontosPadModulo[i] = '.';
    pontosPadModulo[pad] = '\0';
  }
  Serial.printf("[%s][%s][%s][%s%s] %s\n",
                formattedTime, formattedUptime,
                logaGetNivelTxt(nivel), modulo, pontosPadModulo,
                msg);

  // Enviar para fila de log remoto
  if (logaRemotoAtivo() && logQueue)
  {
    LogRemoto log;

    log.timestamp = mktime(&timeinfo);
    log.uptime = uptime;
    log.level = nivel;

    strlcpy(log.modulo, modulo, sizeof(log.modulo));
    strlcpy(log.message, msg, sizeof(log.message));

    // NÃO bloquear caso a fila esteja cheia
    xQueueSend(logQueue, &log, 0);
  }
}

void logaTitulo(const char *msg)
{
  loga("eTomada", LOG_AVISO, "\n====\n== %s ==\n====\n", msg);
}

static void logRemotoTask(void *param)
{
  LogRemoto log;

  while (true)
  {
    if (xQueueReceive(logQueue, &log, portMAX_DELAY) != pdTRUE)
      continue;

    if (!logaRemotoAtivo())
    {
      // Nao deve entrar aqui, essa task nao roda se nao estiver ativo
      // Serial.println("Descartando log remoto!!??????");
      continue;
    }

    // Sem WiFi: simplesmente descarta este log
    if (WiFi.status() != WL_CONNECTED)
    {
      // Serial.println("Descartando log remoto!!");
      continue;
    }

    HTTPClient http;
    http.setTimeout(1000);

    String serverURL = "http://" + logServer + "/api/log";
    if (!http.begin(serverURL))
      continue;

    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["deviceID"] = eTomadaDeviceID();

    doc["timestamp"] = log.timestamp;
    doc["uptime"] = log.uptime;

    doc["level"] = log.level;
    doc["module"] = log.modulo;
    doc["message"] = log.message;

    String body;
    serializeJson(doc, body);

    esp_task_wdt_reset(); // alimenta o watchdog
    int status = http.POST(body);
    esp_task_wdt_reset(); // alimenta o watchdog

    if (status != 200)
    {
      Serial.printf(">>>> POST de log remoto FALHOU! [%d]\n", status);

      String respBody = http.getString(); // TODO :: perigoso!

      Serial.printf("\n +++++>> RESP: %s\n\n", respBody.c_str());
    }

    http.end();
  }
}
