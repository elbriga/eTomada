#include <Arduino.h>
#include <esp_task_wdt.h>

#include "eTomada.h"
#include "loga.h"
#include "wifi.h"
#include "display.h"
#include "ntp.h"
#include "http.h"
#include "regras.h"
#include "sensores.h"

// Timestamp da proxima sincronizacao do NTP
static long ntpSyncTimeTS = 0;

void setup() {
  Serial.begin(115200);

  delay(500);

  logaTitulo("eTomada");

  // WDT : 15 segundos de timeout
  esp_task_wdt_init(15, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);      // adiciona a task atual (loop)

  displayInit();

  bool FSOK = !!LittleFS.begin(true);
  if (!FSOK) {
    logaTitulo("Erro LittleFS - Desativando Servidor Web");
  }

  eTomadaInit();

  displayMostraString(0, 20, "Conectando...");
  WiFiConnect();

  // NTP somente no modo STA
  if (!WiFiGetModoAP()) {
    displayMostraString(0, 40, "Buscando Hora...");
    ntpSyncTimeTS = ntpSyncTime();
  }
  
  if (FSOK) {
    // TODO :: Iniciar o server assim que conectar, mostrar INICIALIZANDO
    httpServerInit();
  }

  logaTitulo("Setup OK!");
}

String getDiaSemana(struct tm timeinfo) {
  switch (timeinfo.tm_wday) {
    case 0: return "Dom";
    case 1: return "Seg";
    case 2: return "Ter";
    case 3: return "Qua";
    case 4: return "Qui";
    case 5: return "Sex";
    case 6: return "Sab";
    default: return "---";
  }
}

int lastSecond = -1;
int last10Second = -1;
void loop() {
  esp_task_wdt_reset(); // alimenta o watchdog

  if (WiFiGetModoAP()) {
    WiFiModoAPLoop();

    // No modo AP não processa as regras
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }

  struct tm timeinfo;
  ntpGetTime(&timeinfo);

  // 1s/1s
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;

    // 10s/10s
    if ((int)(timeinfo.tm_sec / 10) != last10Second) {
      last10Second = timeinfo.tm_sec / 10;

      sensoresAtualiza();

      processaRegras();
    }

    if (displayPodeMostrar()) {
      // Atualizar o relogio
      char formattedTime[10];
      char msgDataHora[32];
      //strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
      strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
      snprintf(msgDataHora, sizeof(msgDataHora), "  %s    %s", getDiaSemana(timeinfo).c_str(), formattedTime);
      displayMostraMsg(msgDataHora, 0, false);
    }

    // Verificar o WiFi
    if (WiFi.status() != WL_CONNECTED) {
      logaTitulo("WiFi caiu!! Reconectar...");
      displayMostraMsg("Reconectando...", 6000);
      WiFiConnect();
    }

    if (ntpSyncTimeTS > 0 && (long)(millis() - ntpSyncTimeTS) >= 0) {
      ntpSyncTimeTS = ntpSyncTime();
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}
