#include <Arduino.h>
#include <stdarg.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "eTomada.h"
#include "loga.h"
#include "ntp.h"

#define LOG_QUEUE_SIZE 32
#define LOG_MESSAGE_SIZE 512

#define LOG_SERVER_URL "http://192.168.1.220:8080/api/log"

struct LogRemoto
{
  time_t timestamp;
  uint32_t uptime; // em segundos -> TODO :: mudar para ms?
  char level[8];
  char modulo[16];
  char message[LOG_MESSAGE_SIZE];
};

static QueueHandle_t logQueue = nullptr;

static void logRemotoTask(void *param);
void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args);

// Função de log para esta modulo
void logaM(LogLevel nivel, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  logaV("LOGS", nivel, fmt, args);
  va_end(args);
}

void logaInit()
{
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

  logaM(LOG_NORMAL, "Log remoto inicializado");
}

// Nova função de logs
void loga(const char *modulo, LogLevel nivel, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  logaV(modulo, nivel, fmt, args);

  va_end(args);
}

// Atalho legado
void logaMensagem(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  logaV("eTomada", LOG_NORMAL, fmt, args);

  va_end(args);
}

const char *logaGetNivelTxt(LogLevel nivel)
{
  switch (nivel)
  {
  case LOG_DESATIVADO:
    return "!OFF!!!";
  case LOG_CRITICO:
    return "!CRIT!!";
  case LOG_AVISO:
    return "AVISO";
  case LOG_NORMAL:
    return "NORMAL";
  case LOG_TESTE:
    return "TESTE";
  case LOG_DEBUG:
    return "DEBUG";
  default:
    return "???";
  }
}

void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args)
{
  // Gerar o log
  char msg[LOG_MESSAGE_SIZE];
  vsnprintf(msg, sizeof(msg), fmt, args);

  // Obter horario
  struct tm timeinfo;
  ntpGetTime(&timeinfo);

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

  Serial.printf("[%s][%s][%s][%s] %s\n",
                formattedTime, formattedUptime,
                logaGetNivelTxt(nivel), modulo,
                msg);

  // Enviar para fila de log remoto
  if (logQueue)
  {
    LogRemoto log;

    log.timestamp = mktime(&timeinfo);
    log.uptime = uptime;

    strlcpy(log.modulo, modulo, sizeof(log.modulo));
    strlcpy(log.level, logaGetNivelTxt(nivel), sizeof(log.level));
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

    // Sem WiFi: simplesmente descarta este log
    if (WiFi.status() != WL_CONNECTED)
    {
      // Serial.println("Descartando log remoto!!");
      continue;
    }

    HTTPClient http;
    http.setTimeout(1000);

    if (!http.begin(LOG_SERVER_URL))
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

    int status = http.POST(body);

    http.end();

    // Por enquanto não fazemos nada com o status.
    //(void)status;
  }
}
