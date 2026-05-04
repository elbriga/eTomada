#include <Arduino.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <Preferences.h>

// WiFi credentials.
const char* WIFI_SSID = "Ligga Gabriel 2.4g";
const char* WIFI_PASS = "09876543";
IPAddress ipStr;

// Reles 1 a 8
#define RELE_LUZ  1
int pinosReles[8] = { 15, 13, 12, 14, -1,-1,-1,-1 };
bool estadoReles[8] = { 0,0,0,0,0,0,0,0 };

// Salvar as regras na memoria FLASH
Preferences prefs;
String strRegraLuz = "";

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


// ==============================================================================================
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
    delay(500);
  }
}

tm printTime(int px, int py) {
  struct tm timeinfo;
  char formattedTime[80];

  if (!getLocalTime(&timeinfo, 2000)) {
    Serial.println("Failed to obtain time");
    //return NULL;
  }
  
  //strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
  tft.drawString(px, py, String(formattedTime));
  
  return timeinfo;
}

void controlaRele(int numRele, bool estado) {
  if (numRele < 1 || numRele > 8) {
    Serial.printf("controlaRele: numRele [%d] invalido!\n", numRele);
    return;
  }

  int r = numRele - 1;
  if (estado != estadoReles[r]) {
    Serial.printf("%s rele %d\n", (estado ? "Ligando" : "Desligando"), numRele);
    digitalWrite(pinosReles[r], estado);
    estadoReles[r] = estado;
  }
}

void loadRegras() {
  Serial.println("Carregando Regras:");

  prefs.begin("regras", false);

  strRegraLuz = prefs.getString("luz", "");
  if (strRegraLuz == "") {
    // Inicializar as regras Default
    strRegraLuz = "OF=02:00-07:59"; // Desligado das 2h as 8h
    prefs.putString("luz", strRegraLuz);
  }
  Serial.printf("> LUZ: [%s]\n", strRegraLuz.c_str());

  Serial.println("");
}

void checkRegra_Luz(tm timeinfo) {
  if (strRegraLuz.length() < 10) {
    // ERRO! Nao deve cair aqui!
    Serial.println("REGRA DE LUZ INVALIDA!!!");
    return;
  }

  const char* regraLuz = strRegraLuz.c_str();
  if (regraLuz[2] != '=' || regraLuz[5] != ':' || regraLuz[8] != '-' || regraLuz[11] != ':') {
    Serial.println("REGRA DE LUZ INVALIDA 2!!!");
    return;
  }
  
  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligaLuz[3] = {0};
  int lidos = sscanf(regraLuz, "%2[^=]=%d:%d-%d:%d", ligaLuz, &hI, &mI, &hF, &mF);
  if (lidos != 5) {
    Serial.println("REGRA DE LUZ INVALIDA 3!!!");
    return;
  }
  if (hI < 0 || hI > 23) {
    Serial.println("REGRA DE LUZ INVALIDA 4!!! hI");
    return;
  }
  if (mI < 0 || mI > 59) {
    Serial.println("REGRA DE LUZ INVALIDA 5!!! mI");
    return;
  }
  if (hF < 0 || hF > 23) {
    Serial.println("REGRA DE LUZ INVALIDA 6!!! hF");
    return;
  }
  if (mF < 0 || mF > 59) {
    Serial.println("REGRA DE LUZ INVALIDA 7!!! mF");
    return;
  }

  int tsI = hI * 60 + mI;
  int tsF = hF * 60 + mF;
  if (tsF <= tsI) {
    Serial.println("REGRA DE LUZ INVALIDA 8!!! (tsF >= tsI)");
    return;
    // TODO flag = 1 E tsF += 24 * 60
  }

  bool acaoEhLigar = !strncmp(ligaLuz, "ON", 2);

  int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool estaNoIntervalo = (tsAgora >= tsI && tsAgora <= tsF);
  if (!acaoEhLigar) estaNoIntervalo = !estaNoIntervalo;

  //Serial.printf("tsI=%d\ntsF=%d\ntsAgora=%d\n", tsI, tsF, tsAgora);

  if (estaNoIntervalo) Serial.println("LIGAR luz!");
  else                 Serial.println("DESLIGAR luz!");

  controlaRele(RELE_LUZ, estaNoIntervalo);
}

void onChangeMinute(tm timeinfo) {
  Serial.printf("[%d:%d] > ", timeinfo.tm_hour, timeinfo.tm_min);

  checkRegra_Luz(timeinfo);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n== eTomada ==\n");

  for(int r=0; r < 8; r++) {
    int pino = pinosReles[r];
    if (pino == -1) {
      // Rele Desativado
      continue;
    }
    pinMode(pino, OUTPUT);
    digitalWrite(pino, estadoReles[r]);
  }

  tft.init();
  tft.clear();
  tft.setFont(ArialMT_Plain_16);
  tft.drawString(30, 0, "eTomada!");
  tft.display();

  loadRegras();

  tft.drawString(0, 20, "Conectando...");
  tft.display();

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
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Falha!! Cheque a configuracao!!");
      Serial.println();
    }
    delay(5000);
  }
  Serial.println("OK!");

  ipStr = WiFi.localIP();
  Serial.print("Endereço IP: ");
  Serial.println(ipStr);
  
  Serial.print("NTP: ");
  tft.drawString(0, 40, "NTP Init...");
  tft.display();
  syncTime();
  Serial.println("OK!");
  
  delay(100);
  Serial.println("");
}

int lastMinute = -1;
void loop() {
  struct tm timeinfo;

  tft.clear();

  tft.drawString(30, 0, "eTomada!");
  timeinfo = printTime(36, 20);
  tft.drawString(0, 40, ipStr.toString());
  
  tft.display();

  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;
    onChangeMinute(timeinfo);
  }

  delay(1000);
}
