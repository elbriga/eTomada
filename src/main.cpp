#include <Arduino.h>
#include <esp_task_wdt.h>

#include "eTomada.h"
#include "loga.h"
#include "wifi.h"
#include "display.h"
#include "ntp.h"
#include "http.h"
#include "rele.h"
#include "regra.h"

// TODO :: Alterar para rele.h > getRele(r), para nao depender desta global ??
Rele reles[8] = {
  // Valores default
  { 15, 0, "Luz", "OF=02:00-07:59" },
  { 13, 0, "Umidificador", "" },
  { 12, 0, "Ventilador", "" },
  { 14, 0, "Desumidificador", "" },
  { -1, 0, "", "" },
  { -1, 0, "", "" },
  { -1, 0, "", "" },
  { -1, 0, "", "" }
};

// Display OLED
SSD1306Wire tft(I2C_DISPLAY_ADDR, SDA, SCL);

// FS - LittleFS
bool FSOK = false;

// ==============================================================================================
void processaRegras() {
  time_t agora = time(nullptr);
  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);

  Rele *rele;
  for (int r=0; r < 8; r++) {
    rele = &reles[r];
    if (rele->regra == "") continue;

    String msg = checkRegra(r, timeinfo);
    if (msg != "") {
      logaMensagem("%s", msg.c_str());
      displayMostraMsg(msg.c_str(), 5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.println("\n== eTomada ==\n");

  // WDT : 15 segundos de timeout
  esp_task_wdt_init(15, true); // true = resetar automaticamente
  esp_task_wdt_add(NULL);      // adiciona a task atual (loop)

  for(int r=0; r < 8; r++) {
    Rele *rele = &reles[r];
    if (rele->pino == -1) {
      // Rele Desativado
      continue;
    }
    pinMode(rele->pino, OUTPUT);
    digitalWrite(rele->pino, rele->estado);
  }

  displayInit();

  FSOK = !!LittleFS.begin(true);
  if (!FSOK) {
    Serial.println("Erro LittleFS - Desativando File-system");
  }

  eTomadaLoadConfig();

  tft.drawString(0, 20, "Conectando...");
  tft.display();
  WiFiConnect();

  Serial.print("NTP: ");
  tft.drawString(0, 40, "Buscando Hora...");
  tft.display();
  ntpSyncTime();

  time_t agora = time(nullptr);
  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);

  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Serial.println(formattedTime);
  
  delay(100);

  if (FSOK) {
    httpServerInit(processaRegras);
  }

  logaMensagem("Setup() OK!");
  Serial.println("");
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

int lastMinute = -1;
int lastSecond = -1;
void loop() {
  esp_task_wdt_reset(); // alimenta o watchdog

  time_t agora = time(nullptr);
  struct tm timeinfo;
  localtime_r(&agora, &timeinfo);

  if (timeinfo.tm_sec != lastSecond && displayPodeMostrar()) {
    lastSecond = timeinfo.tm_sec;

    // Atualizar o relogio
    char formattedTime[10];
    char msgDataHora[32];
    //strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
    sprintf(msgDataHora, "  %s    %s", getDiaSemana(timeinfo).c_str(), formattedTime);
    displayMostraMsg(msgDataHora);
  }

  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;

    processaRegras();

    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
      // Ao mudar de dia - sync NTP de novo
      ntpSyncTime();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    logaMensagem("WiFi caiu!! Reconectar...");
    displayMostraMsg("Reconectando...", 10000);
    WiFiConnect();
  }

  vTaskDelay(1);
}
