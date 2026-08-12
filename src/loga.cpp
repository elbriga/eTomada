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
  char timestamp[32];
  char message[LOG_MESSAGE_SIZE];
  uint32_t uptime;
};

static QueueHandle_t logQueue = nullptr;

static void logRemotoTask(void *param);

void logaInit()
{
  if (logQueue)
  {
    logaMensagem("ERRO: logaInit() chamado duas vezes?");
    return;
  }

  logQueue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogRemoto));
  if (!logQueue)
  {
    logaMensagem("ERRO: nao foi possivel criar fila de logs");
    return;
  }

  xTaskCreate(
      logRemotoTask,
      "logRemoto",
      4096,
      nullptr,
      1,
      nullptr);

  logaMensagem("Log remoto inicializado");
}

void logaMensagem(const char *fmt, ...)
{
  char msg[LOG_MESSAGE_SIZE];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

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

  Serial.printf("[%s][%s] %s\n", formattedTime, formattedUptime, msg);

  // Enviar para fila de log remoto
  if (logQueue)
  {
    LogRemoto log;

    strlcpy(log.timestamp, formattedTime, sizeof(log.timestamp));
    strlcpy(log.message, msg, sizeof(log.message));
    log.uptime = uptime;

    // NÃO bloquear caso a fila esteja cheia
    xQueueSend(logQueue, &log, 0);
  }
}

void logaTitulo(const char *msg)
{
  logaMensagem("\n====\n== %s ==\n====\n", msg);
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
      // Serial.println("......>>>>> Descartando log remoto!!");
      continue;
    }

    HTTPClient http;
    http.setTimeout(1000);

    if (!http.begin(LOG_SERVER_URL))
      continue;

    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["deviceID"] = eTomadaDeviceID();
    doc["level"] = "INFO";
    doc["module"] = "etomada";
    doc["message"] = log.message;
    doc["timestamp"] = log.timestamp;
    doc["uptime"] = log.uptime;

    String body;
    serializeJson(doc, body);

    // Serial.printf("......>>>>>..... [%s]\n", body.c_str());
    int status = http.POST(body);

    http.end();

    // Por enquanto não fazemos nada com o status.
    //(void)status;
  }
}
