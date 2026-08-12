#include <Arduino.h>
#include <esp_task_wdt.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "eTomada.h"
#include "mestre.h"
#include "loga.h"
#include "wifi.h"
#include "display.h"
#include "ntp.h"
#include "http.h"
#include "regras.h"
#include "sensor.h"
#include "botao.h"
#include "discover.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MAIN", nivel, fmt, ##__VA_ARGS__)

#define FS_LIMITE_LIVRE 100 * 1024 // Para LOG de aviso de FS cheio

// Timestamp da proxima sincronizacao do NTP
static long ntpSyncTimeTS = 0;

void setup()
{
  Serial.begin(115200);

  delay(500);

  // Inicializa a Task de logs remotos, ela ira descartar logs enquanto sem wifi
  logaInit();

  logaTitulo("eTomada");

  // WDT : 5 segundos de timeout
  esp_task_wdt_init(5, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);     // adiciona a task atual (loop)

  nvs_stats_t stats;
  nvs_get_stats(NULL, &stats);
  logaM(LOG_NORMAL, "Inicializando Preferences: (used:%d, free:%d)",
        stats.used_entries, stats.free_entries);

  displayInit();

  logaM(LOG_NORMAL, "Inicializando FS:");
  if (!LittleFS.begin())
    utilDIE("ERRO LITTLEFS!!!");

  displayMostraString(0, 20, "Conectando...");
  WiFiConnect();

  // NTP somente no modo STA
  if (!WiFiGetModoAP())
  {
    displayMostraString(0, 40, "Buscando Hora...");
    ntpSyncTimeTS = ntpSyncTime();
  }

  // Mostrar o status do FS
  {
    size_t total = LittleFS.totalBytes();
    size_t usado = LittleFS.usedBytes();
    size_t livre = total - usado;
    logaM(LOG_NORMAL, "  Total : %u bytes (%u KB)", total, total / 1024);
    logaM(LOG_NORMAL, "  Usado : %u bytes (%u KB)", usado, usado / 1024);
    logaM(LOG_NORMAL, "  Livre : %u bytes (%u KB)", livre, livre / 1024);
    if (livre < FS_LIMITE_LIVRE)
    {
      logaM(LOG_CRITICO, ">>> POUCO ESPAÇO NO FILE SYSTEM!!!");
      logaM(LOG_CRITICO, ">>> POUCO ESPAÇO NO FILE SYSTEM!!!");
    }
  }

  eTomadaInit();

  logaM(LOG_NORMAL, "Inicializando o servidor http:");
  httpServerInit();

  logaTitulo("Setup OK!");
}

const char *getDiaSemana(struct tm timeinfo)
{
  switch (timeinfo.tm_wday)
  {
  case 0:
    return "Dom";
  case 1:
    return "Seg";
  case 2:
    return "Ter";
  case 3:
    return "Qua";
  case 4:
    return "Qui";
  case 5:
    return "Sex";
  case 6:
    return "Sab";
  default:
    return "---";
  }
}

int wifiFora = 0;
int lastSecond = -1;
int last10Second = -1;
int lastMinute = -1;
void loop()
{
  esp_task_wdt_reset(); // alimenta o watchdog

  if (WiFiGetModoAP())
    WiFiModoAPLoop();
  else
    discoverLoop();

  // 10ms/10ms
  botoesAtualiza();

  struct tm timeinfo;
  ntpGetTime(&timeinfo);

  // 1s/1s
  if (timeinfo.tm_sec != lastSecond)
  {
    lastSecond = timeinfo.tm_sec;

    // TODO : #ifdef TEM_OLED
    if (displayPodeMostrar())
    {
      // Atualizar o relogio
      char formattedTime[10];
      char msgDataHora[32];
      // strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
      strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
      snprintf(msgDataHora, sizeof(msgDataHora), "  %s    %s", getDiaSemana(timeinfo), formattedTime);
      displayMostraMsg(msgDataHora, 0, false);
    }

    // 10s/10s
    if ((int)(timeinfo.tm_sec / 10) != last10Second)
    {
      last10Second = timeinfo.tm_sec / 10;

      sensoresAtualiza();

      // Se estivermos no modo MODO_NO essa funcao retorna sem fazer nada
      // regrasProcessa();

      // Keepalive para a interface web
      httpEnviaSSE("{}", "sse_ping");

      // Verificar os NÓs remotos (em nova Task):
      if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
      {
        nodosRemotosRefresh();
      }

      mestreLoop();

      // 1m/1m
      if (timeinfo.tm_min != lastMinute)
      {
        lastMinute = timeinfo.tm_min;
        eventoPost(EVENTO_HORARIO, nullptr, false, false);
      }
    }

    // Verificar o WiFi
    if (!WiFiGetModoAP())
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        wifiFora++;
        if (wifiFora > 5)
        {
          logaTitulo("WiFi caiu!! Reconectar...");
          displayMostraMsg("Reconectando...", 6000);
          WiFiConnect();
        }
      }
      else
      {
        wifiFora = 0;
      }

      // Sync NTP
      if (ntpSyncTimeTS > 0 && (long)(millis() - ntpSyncTimeTS) >= 0)
      {
        ntpSyncTimeTS = ntpSyncTime();
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}
