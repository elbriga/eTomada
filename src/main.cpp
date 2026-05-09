#include <Arduino.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <stdarg.h>

// WiFi credentials.
const char* WIFI_SSID = "Ligga Gabriel 2.4g";
const char* WIFI_PASS = "09876543";
IPAddress ipStr;

struct Rele {
  int pino;
  bool estado;
  String nome;
  String regra;
};

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

// Salvar as regras na memoria FLASH
Preferences prefs;

// Display OLED
#define I2C_DISPLAY_ADDR    0x3C
#define SDA                 5
#define SCL                 4
SSD1306Wire tft(I2C_DISPLAY_ADDR, SDA, SCL);

// NTP
const char* ntpServer1 = "ntp.br";
const char* ntpServer2 = "pool.ntp.org";
// Time zone string
// See list of timezone strings https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* tzInfo = "<-03>3";

// FS - LittleFS
bool FSOK = false;

// Web Server
AsyncWebServer httpServer(80);

// DataHora Global do sistema, atualizada no loop()
time_t agora;
struct tm timeinfo;

// ==============================================================================================
void loga(const char* fmt, ...) {
  char msg[512];

  // Formatar mensagem variadica
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Obter horario
  char formattedTime[32] = {0};
  strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);

  // Log final
  Serial.printf("[%s] %s\n", formattedTime, msg);
}

unsigned long timeoutMsg = 0;
void mostraMsg(String msg, int timeout = 0) {
  tft.clear();
  tft.drawString(30, 0, "eTomada!");
  tft.drawString(0, 20, msg);
  tft.drawString(0, 40, ipStr.toString());
  tft.display();

  if (timeout > 0)
    timeoutMsg = millis() + timeout;
}

void syncTime() {
  // Start NTP using the two servers above
  configTime(0, 0, ntpServer1, ntpServer2);

  // Set the timezone for your region
  setenv("TZ", tzInfo, 1);
  tzset();

  // Wait until a valid time is received from the NTP server
  // 1577836800 is the Unix time for Jan 1, 2020
  time_t now = 0;
  while (time(&now) < 1577836800) {
    esp_task_wdt_reset(); // alimenta o watchdog
    delay(500);
  }
}

String controlaRele(int numRele, bool estado) {
  if (numRele < 1 || numRele > 8) {
    Serial.printf("controlaRele: numRele [%d] invalido!\n", numRele);
    return "";
  }
  
  Rele *rele = &reles[numRele - 1];
  if (rele->pino == -1) {
    Serial.printf("controlaRele[%d]: pino invalido!\n", numRele);
    return "";
  }

  String ret = "";
  if (estado != rele->estado) {
    digitalWrite(rele->pino, estado);
    rele->estado = estado;

    char msg[128];
    sprintf(msg, "%s %s (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"), rele->nome.c_str(), numRele, rele->pino);
    ret = msg;
  }

  return ret;
}

String getPrefsAtr(int num, String nomeAtr) {
  char buff[10];
  sprintf(buff, "%s%d", nomeAtr.c_str(), num);
  return prefs.isKey(buff) ? prefs.getString(buff, "") : "";
}

void setPrefsAtr(int num, String nomeAtr, String val) {
  char buff[10];
  sprintf(buff, "%s%d", nomeAtr.c_str(), num);
  prefs.putString(buff, val);
}

void loadConfig() {
  Serial.println("Carregando Configuracao dos reles:");

  prefs.begin("reles", false);

  // Para testes
  // prefs.putString("nome1", "Luz");
  // prefs.putString("regra1", "OF-02:00-07:59");
  // prefs.putString("pino1", "15");

  for (int r=0; r < 8; r++) {
    Rele *rele = &reles[r];

    rele->nome  = getPrefsAtr(r+1, "nome");
    rele->regra = getPrefsAtr(r+1, "regra");
    rele->pino  = atoi(getPrefsAtr(r+1, "pino").c_str());

    Serial.printf("Rele %d:%d (%s) > [%s]\n", r+1, rele->pino, rele->nome.c_str(), rele->regra.c_str());
  }

  Serial.println("");
}

String validaHora(String hora) {
  int h, m;
  int lidos = sscanf(hora.c_str(), "%d:%d", &h, &m);
  if (lidos != 2) {
    return "campos";
  }
  if (h < 0 || h > 23) {
    return "hora";
  }
  if (m < 0 || m > 59) {
    return "minuto";
  }
  return "";
}

String validaRegra(String regra) {
  if (regra.length() < 10) {
    return "len";
  }

  char acao[3] = {0};
  char param1[17] = {0};
  char param2[17] = {0};
  int lidos = sscanf(regra.c_str(), "%2[^-]-%16[^-]-%16[^-]", acao, param1, param2);
  if (lidos < 3) {
    return "campos:" + String(lidos) + ":" + String(acao) + ":" + String(param1) + ":" + String(param2);
  }

  // Validar as Acoes
  if (!strncmp(acao, "ON", 2) || !strncmp(acao, "OF", 2)) {
    String hOK = validaHora(param1);
    if (hOK != "") {
      return "hI:" + hOK;
    }
    hOK = validaHora(param2);
    if (hOK != "") {
      return "hF:" + hOK;
    }
  } else {
    return "acao";
  }

  return "";
}

String checkRegra(int num) {
  Rele *rele = &reles[num];

  String regraOK = validaRegra(rele->regra);
  if (regraOK != "") {
    return "Regra[" + String(num+1) + "] Invalida:" + regraOK;
  }

  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligar[3] = {0};
  sscanf(rele->regra.c_str(), "%2[^-]-%d:%d-%d:%d", ligar, &hI, &mI, &hF, &mF);

  int tsI = hI * 60 + mI;
  int tsF = hF * 60 + mF;
  int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool virouDia = tsF < tsI;
  bool estaNoIntervalo = !virouDia ?
    (tsAgora >= tsI && tsAgora <= tsF) :
    (tsAgora >= tsI || tsAgora <= tsF);

  bool acaoEhLigar = !strncmp(ligar, "ON", 2);
  if (!acaoEhLigar) estaNoIntervalo = !estaNoIntervalo;

  // Serial.printf("tsI=%d\ntsF=%d\ntsAgora=%d\n", tsI, tsF, tsAgora);
  // if (estaNoIntervalo) Serial.printf("LIGAR [%s]\n", nomeReles[num]);
  // else                 Serial.printf("DESLIGAR [%s]\n", nomeReles[num]);

  return controlaRele(num + 1, estaNoIntervalo);
}

void onChangeMinute() {
  Rele *rele;

  for (int r=0; r < 8; r++) {
    rele = &reles[r];
    if (rele->regra == "") continue;

    String msg = checkRegra(r);
    if (msg != "") {
      loga("%s", msg.c_str());
      mostraMsg(msg, 5000);
    }
  }
}

void logaRequest(AsyncWebServerRequest *request, String resultado) {
  loga("[org:%s] %s %s => [%s]",
    request->client()->remoteIP().toString(),
    request->methodToString(),
    request->url().c_str(),
    resultado.c_str());
}

void httpServerInit() {
//#ifdef DEV   TODO
  // Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
//#endif

  httpServer.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["datahora"] = (unsigned long)agora;

    char formattedTime[32];
    strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
    doc["datahorastr"] = formattedTime;

    JsonArray arr = doc["reles"].to<JsonArray>();
    for (int i = 0; i < 8; i++) {
        JsonObject r = arr.add<JsonObject>();
        Rele *rele = &reles[i];
        r["nome"]   = rele->nome;
        r["regra"]  = rele->regra;
        r["pino"]   = rele->pino;
        r["estado"] = rele->estado;
    }

    String out;
    serializeJson(doc, out);

    logaRequest(request, "200 OK");
    request->send(200, "application/json", out);
  });

  httpServer.on("/api/setReleConfig",
    HTTP_PUT,
    [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request,
    uint8_t *data,
    size_t len,
    size_t index,
    size_t total)
  {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) {
      logaRequest(request, "400 JSON invalido");
      request->send(400, "text/plain", "JSON invalido");
      return;
    }

    int numRele = doc["rele"];
    if (numRele < 1 || numRele > 8) {
      logaRequest(request, "400 Rele invalido");
      request->send(400, "text/plain", "Rele invalido");
      return;
    }

    String novaRegra = doc["regra"];
    if (!novaRegra) {
      logaRequest(request, "400 Regra Zerada");
      request->send(400, "text/plain", "Regra Zerada");
      return;
    }

    String regraOK = validaRegra(novaRegra);
    if (regraOK != "") {
      logaRequest(request, "400 Regra Invalida :: " + regraOK);
      request->send(400, "text/plain", "Regra Invalida :: " + regraOK);
      return;
    }

    Rele *rele = &reles[numRele - 1];
    String nome  = rele->nome;
    String regra = rele->regra;
    int    pino  = rele->pino;
    rele->nome  = doc["nome"]  | rele->nome;
    rele->regra = doc["regra"] | rele->regra;
    rele->pino  = doc["pino"]  | rele->pino;

    logaRequest(request, "200 OK");
    
    Serial.println(">> RELE " + String(numRele) + " nome:" + rele->nome +
        " regra:" + rele->regra + " pino:" + String(rele->pino));

    // Setar no prefs
    if (rele->nome != nome) {
      setPrefsAtr(numRele, "nome",  rele->nome);
    }
    if (rele->regra != regra) {
      setPrefsAtr(numRele, "regra", rele->regra);
    }
    if (rele->pino != pino) {
      setPrefsAtr(numRele, "pino",  String(rele->pino));
      pinMode(rele->pino, OUTPUT);
      // Desligar pino antigo
      digitalWrite(pino, LOW);
    }

    // Checar as regras novamente
    onChangeMinute();

    request->send(200, "application/json", "{\"msg\": \"OK\"}");
  });

  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  httpServer.onNotFound([](AsyncWebServerRequest *request) {
    // Tratar o OPTIONS
    if (request->method() == HTTP_OPTIONS) {
      logaRequest(request, "OPTIONS");
      request->send(200);
    } else {
      logaRequest(request, "404 Not Found");
      request->send(404);
    }
  });
  
  httpServer.begin();
}

void WiFiConnect() {
  // Connect to Wifi.
  Serial.print("Conectando a rede [");
  Serial.print(WIFI_SSID);
  Serial.print("]: ");

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    esp_task_wdt_reset(); // alimenta o watchdog

    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Falha!! Cheque a configuracao!!");
      Serial.println();
      delay(5000);
    }
    delay(500);
  }
  Serial.println("OK!");

  ipStr = WiFi.localIP();
  Serial.print("Endereço IP: ");
  Serial.println(ipStr);
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

  tft.init();
  tft.clear();
  tft.setFont(ArialMT_Plain_16);
  tft.drawString(30, 0, "eTomada!");
  tft.display();

  FSOK = !!LittleFS.begin(true);
  if (!FSOK) {
    Serial.println("Erro LittleFS - Desativando File-system");
  }

  loadConfig();

  tft.drawString(0, 20, "Conectando...");
  tft.display();

  WiFiConnect();

  Serial.print("NTP: ");
  tft.drawString(0, 40, "Buscando Hora...");
  tft.display();
  syncTime();

  agora = time(nullptr);
  localtime_r(&agora, &timeinfo);

  char formattedTime[32];
  strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Serial.println(formattedTime);
  
  delay(100);

  if (FSOK) {
    httpServerInit();
  }

  loga("Setup() OK!");
  Serial.println("");
}

String getDiaSemana() {
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

  agora = time(nullptr);
  localtime_r(&agora, &timeinfo);

  if (timeinfo.tm_sec != lastSecond && timeoutMsg < millis()) {
    lastSecond = timeinfo.tm_sec;
    timeoutMsg = 0;

    // Atualizar o relogio
    char formattedTime[10];
    char msgDataHora[32];
    //strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
    sprintf(msgDataHora, "  %s    %s", getDiaSemana().c_str(), formattedTime);
    mostraMsg(msgDataHora);
  }

  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;

    onChangeMinute();

    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
      // Ao mudar de dia - sync NTP de novo
      syncTime();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    loga("WiFi caiu!! Reconectar...");
    mostraMsg("Reconectando...", 10000);
    WiFiConnect();
  }

  vTaskDelay(1);
}
