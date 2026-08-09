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

// Timestamp da proxima sincronizacao do NTP
static long ntpSyncTimeTS = 0;

void setup()
{
  Serial.begin(115200);

  delay(500);

  logaTitulo("eTomada");

  // WDT : 5 segundos de timeout
  esp_task_wdt_init(5, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);     // adiciona a task atual (loop)

  nvs_stats_t stats;
  nvs_get_stats(NULL, &stats);
  logaMensagem("Inicializando Preferences: (used:%d, free:%d)",
               stats.used_entries, stats.free_entries);

  displayInit();

  bool FSOK = !!LittleFS.begin(true);
  if (!FSOK)
  {
    logaTitulo("Erro LittleFS - Desativando Servidor Web");
  }

  displayMostraString(0, 20, "Conectando...");
  WiFiConnect();

  // NTP somente no modo STA
  if (!WiFiGetModoAP())
  {
    displayMostraString(0, 40, "Buscando Hora...");
    ntpSyncTimeTS = ntpSyncTime();
  }

  eTomadaInit();

  if (FSOK)
  {
    httpServerInit();
  }

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

  struct tm timeinfo;
  ntpGetTime(&timeinfo);

  if (WiFiGetModoAP())
  {
    WiFiModoAPLoop();

    if (timeinfo.tm_year < 2026)
    {
      // Sem data/hora não processa as regras
      vTaskDelay(pdMS_TO_TICKS(100));
      return;
    }
  }
  else
  {
    discoverLoop();
  }

  // 10ms/10ms
  botoesAtualiza();

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
