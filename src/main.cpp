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
#include "rtc-hw.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MAIN", nivel, fmt, ##__VA_ARGS__)

#define FS_LIMITE_LIVRE 100 * 1024 // Para LOG de aviso de FS cheio

// Timestamp da proxima sincronizacao do NTP
static long ntpSyncTimeTS = 0;

// Controles do loop principal
static int lastSecond = -1;
static int last10Second = -1;
static int lastMinute = -1;
static int wifiFora = 0;
static int lastMsgDBM = -(60 * 60 * 1000); // MSG a cada 1h

void setup()
{
  Serial.begin(115200);

  delay(500);

  // WDT : 5 segundos de timeout
  esp_task_wdt_init(5, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);     // adiciona a task atual (loop)

  // Inicializa o nivel de LOG e a Task de logs remotos, ela ira descartar logs enquanto sem wifi
  logaInit();

  logaTitulo("eTomada");

  // Verificar se temos RTC
  rtcInit();

  // Inicializar MODO DE OPERAÇÃO e o deviceID
  eTomadaInit0();

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
    // Verificar se ja temos hora do RTC
    struct tm timeinfo;
    sysGetTime(&timeinfo);

    if (timeinfo.tm_year + 1900 < 2026)
    {
      // Nao temos RTC ou ele falhou!
      displayMostraString(0, 40, "Buscando Hora...");
      ntpSyncTimeTS = ntpSyncTime();
    }
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

  // Inicializar controles do loop principal
  struct tm timeinfo;
  sysGetTime(&timeinfo);
  lastSecond = timeinfo.tm_sec;
  last10Second = timeinfo.tm_sec / 10;
  lastMinute = timeinfo.tm_min; // TODO :: Esse impede que dispare um EVENTO_HORARIO para o minuto atual do boot
}

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
  sysGetTime(&timeinfo);

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
      snprintf(msgDataHora, sizeof(msgDataHora), "  %s    %s", utilGetDiaSemana(timeinfo), formattedTime);
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

      // Usado no NODO_NO para verificar se o mestre ficou offline
      mestreLoop();

      // 1m/1m
      if (timeinfo.tm_min != lastMinute)
      {
        lastMinute = timeinfo.tm_min;
        eventoPost(EVENTO_HORARIO, nullptr, false, false);

        int dbm = WiFi.RSSI();
        if (dbm < -70)
        {
          if (millis() - lastMsgDBM > 60 * 60 * 1000)
          {
            logaM(LOG_AVISO, "Sinal do WiFi muito baixo! [%d]", dbm);
            lastMsgDBM = millis();
          }
        }
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
