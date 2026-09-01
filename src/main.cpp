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
#include "util.h"
#include "rtc-hw.h"
#include "hardwareProfile.h"
#include "rgb-led.h"
#include "ota.h"
#include "memoria.h"
#include "shaCache.h"
#include "umidificador.h"
#include "mdns-gs.h"
#include "led.h"
#include "botaoReset.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MAIN", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

#define FS_LIMITE_LIVRE 100 * 1024 // Para LOG de aviso de FS cheio

// Timestamp da proxima sincronizacao do NTP
static long ntpSyncTimeTS = 0;
static bool ntpSyncOKFlag = false; // Flag setada pelo callback do ntp

// Controles do loop principal
static int lastSecond = -1;
static int last10Second = -1;
static int lastMinute = -1;
static int lastHour = -1;
static int wifiFora = 0;
static int lastMsgDBM = -(60 * 60 * 1000); // MSG a cada 1h

void mainNtpSetSyncFlag()
{
  ntpSyncOKFlag = true;
}

void setup()
{
  Serial.begin(115200);

  delay(500);

  // WDT : 5 segundos de timeout
  esp_task_wdt_init(5, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);     // adiciona a task atual (loop)

  ledInit();

  ntpSetTZ(); // Podemos estar no RTC interno que reseta sem config de TZ

  // Inicializa o nivel de LOG e a Task de logs remotos, ela ira descartar logs enquanto sem wifi
  logaInit();

  logaTitulo("eTomada");

  // Para testar o RTC:
  // rtcForceResetSystemTime();

  // Verificar se temos RTC
  rtcInit();

  ntpInit();

  shaInit();

  botaoResetInit();

  // Inicializar MODO DE OPERAÇÃO e o deviceID
  eTomadaInit0();

  logaM(LOG_NORMAL, "Flash: %u MB", ESP.getFlashChipSize() / (1024 * 1024));
  logaM(LOG_NORMAL, "Arduino ESP32: %s", ESP.getSdkVersion());

  nvs_stats_t stats;
  nvs_get_stats(NULL, &stats);
  logaM(LOG_NORMAL, "Inicializando Preferences: (used:%d, free:%d)",
        stats.used_entries, stats.free_entries);

  displayInit();

  logaM(LOG_NORMAL, "Inicializando FS");
  if (!LittleFS.begin())
    utilDIE("ERRO LITTLEFS!!!");

  if (umidificadorAtivo())
    umidificadorInit();

  logaM(LOG_NORMAL, "Inicializando WiFi");
  displayMostraString(145, 61, "Conectando");
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
      displayMostraString(145, 87, "Buscando Hora");
      ntpSyncTimeTS = ntpSyncTime();
    }
    else
    {
      // conferir o NTP em 10 segundos
      ntpSyncTimeTS = millis() + 10 * 1000;
    }
  }

  // Mostrar o status do FS
  {
    size_t total = LittleFS.totalBytes();
    size_t usado = LittleFS.usedBytes();
    size_t livre = total - usado;
    logaM(LOG_NORMAL, "Status LittleFS:");
    logaM(LOG_NORMAL, "> Total : %u bytes (%u KB)", total, total / 1024);
    logaM(LOG_NORMAL, "> Usado : %u bytes (%u KB)", usado, usado / 1024);
    logaM(LOG_NORMAL, "> Livre : %u bytes (%u KB)", livre, livre / 1024);
    if (livre < FS_LIMITE_LIVRE)
    {
      logaM(LOG_CRITICO, ">>> POUCO ESPAÇO NO FILE SYSTEM!!!");
      logaM(LOG_CRITICO, ">>> POUCO ESPAÇO NO FILE SYSTEM!!!");
    }
  }

  logaM(LOG_NORMAL, "Versao do Firmware: %s", eTomadaGetVersao());

  logaM(LOG_NORMAL, "Inicializando OTA:");
  otaInit();

  logaM(LOG_NORMAL, "== eTomada Init() ==");
  eTomadaInit();

  logaM(LOG_NORMAL, "Inicializando o servidor http:");
  httpServerInit();

  mdnsInit(true);

  logaTitulo("Setup OK!");

  /*/ Inicializar controles do loop principal
  struct tm timeinfo;
  sysGetTime(&timeinfo);
  lastSecond = timeinfo.tm_sec;
  last10Second = timeinfo.tm_sec / 10;
  lastMinute = timeinfo.tm_min; // TODO :: Esse impede que dispare um EVENTO_HORARIO para o minuto atual do boot
  lastHour = timeinfo.tm_hour; */

  rgbLedSetAnim(0); // Verde == Loop
}

static uint32_t tsMdnsChuncho = 0;

void loop()
{
  esp_task_wdt_reset(); // alimenta o watchdog

  if (WiFiGetModoAP())
    WiFiModoAPLoop();

  // 5ms/5ms
  botoesAtualiza();
  ledProcessa();
  if (botaoResetAtivo())
    botaoResetAtualiza();

  struct tm timeinfo;
  sysGetTime(&timeinfo);

  // 1s/1s
  if (timeinfo.tm_sec != lastSecond)
  {
    lastSecond = timeinfo.tm_sec;

    // CHUNCHO para consertar o mDNS
    if (millis() - tsMdnsChuncho >= 60 * 1000)
    {
      tsMdnsChuncho = millis();
      mdnsInit(false);
    }

    if (ntpSyncOKFlag)
    {
      ntpSyncOKFlag = false;
      logaM(LOG_AVISO, "NTP SYNC OK. Sync RTC");
      rtcStoreSystemClock();
      if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
      {
        // TODO : verificar se deve rodar sempre
        regrasBoot();
      }
    }

#ifdef TELA_COLORIDA
    if (displayPodeMostrar())
    {
      // Atualizar o relogio
      char formattedTime[16];
      // char msgDataHora[32];
      // strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
      strftime(formattedTime, sizeof(formattedTime), "%H : %M : %S", &timeinfo);
      // snprintf(msgDataHora, sizeof(msgDataHora), "%s %s", utilGetDiaSemana(timeinfo), formattedTime);
      displayMostraMsg(formattedTime, 0, false);
    }
#endif

    // 10s/10s
    if ((int)(timeinfo.tm_sec / 10) != last10Second)
    {
      last10Second = timeinfo.tm_sec / 10;

      rgbLedSetAnim(3, 3);
      sensoresAtualiza(); // TODO :: non block!

      // Keepalive para a interface web
      httpEnviaSSE("{}", "sse_ping");

      // Verificar os NÓs remotos (em nova Task) a cada 10s:
      if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
        nodosRemotosRefresh();

      // Usado no NODO_NO para verificar se o mestre ficou offline
      if (eTomadaGetModoOperacao() == MODO_NO)
        mestreLoop();

      // 1m/1m
      if (timeinfo.tm_min != lastMinute)
      {
        lastMinute = timeinfo.tm_min;

        if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
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

        // 1h/1h
        if (timeinfo.tm_hour != lastHour)
        {
          lastHour = timeinfo.tm_hour;
          memoriaLog("1H/1H");
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

  vTaskDelay(pdMS_TO_TICKS(5));
  yield();
}
