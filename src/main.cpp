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
String nomeReles[8] = { "Luz", "Umidificador", "Ventilador", "Desumidificador", "","","","" };
String regraReles[8] = { "OF=02:00-07:59", "","","","","","","" }; // Desligado das 2h as 8h

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

String controlaRele(int numRele, bool estado) {
  if (numRele < 1 || numRele > 8) {
    Serial.printf("controlaRele: numRele [%d] invalido!\n", numRele);
    return "";
  }

  String ret = "";

  int r = numRele - 1;
  if (estado != estadoReles[r]) {
    digitalWrite(pinosReles[r], estado);
    estadoReles[r] = estado;

    char msg[128];
    sprintf(msg, "%s rele %d (%s) pino %d", (estado ? "Ligando" : "Desligando"), numRele, nomeReles[r].c_str(), pinosReles[r]);
    ret = msg;
  }

  return ret;
}

void loadRegras() {
  Serial.println("Carregando Regras:");

  prefs.begin("regras", false);

  // para testes : prefs.putString("regra1", "OF=02:00-07:59");

  char nomeAtr[10];
  for (int r=0; r < 8; r++) {
    String def = regraReles[r];
    sprintf(nomeAtr, "regra%d", r + 1);
    regraReles[r] = prefs.isKey(nomeAtr) ? prefs.getString(nomeAtr, "") : "";
    if (regraReles[r] == "") {
      if (def == "") {
        continue;
      }
      // Inicializar as regras Default
      regraReles[r] = def;
      prefs.putString(nomeAtr, def);
    }
    Serial.printf("Rele %d (%s) > [%s]\n", r+1, nomeReles[r].c_str(), regraReles[r].c_str());
  }

  Serial.println("");
}

String checkRegra(int num, tm timeinfo) {
  String strRegra = regraReles[num];

  if (strRegra == "") {
    return "";
  }

  if (strRegra.length() < 10) {
    // ERRO! Nao deve cair aqui!
    Serial.printf("REGRA RELE %d INVALIDA!!!", num+1);
    return "";
  }

  const char* regraCSTR = strRegra.c_str();
  if (regraCSTR[2] != '=' || regraCSTR[5] != ':' || regraCSTR[8] != '-' || regraCSTR[11] != ':') {
    Serial.printf("REGRA RELE %d INVALIDA!!! Formato", num+1);
    return "";
  }
  
  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligar[3] = {0};
  int lidos = sscanf(regraCSTR, "%2[^=]=%d:%d-%d:%d", ligar, &hI, &mI, &hF, &mF);
  if (lidos != 5) {
    Serial.printf("REGRA RELE %d INVALIDA!!! Campos", num+1);
    return "";
  }
  if (hI < 0 || hI > 23) {
    Serial.printf("REGRA RELE %d INVALIDA!!! hI", num+1);
    return "";
  }
  if (mI < 0 || mI > 59) {
    Serial.printf("REGRA RELE %d INVALIDA!!! mI", num+1);
    return "";
  }
  if (hF < 0 || hF > 23) {
    Serial.printf("REGRA RELE %d INVALIDA!!! hF", num+1);
    return "";
  }
  if (mF < 0 || mF > 59) {
    Serial.printf("REGRA RELE %d INVALIDA!!! mF", num+1);
    return "";
  }

  int tsI = hI * 60 + mI;
  int tsF = hF * 60 + mF;
  if (tsF <= tsI) {
    Serial.printf("REGRA RELE %d INVALIDA!!! (tsF >= tsI)", num+1);
    return "";
    // TODO flag = 1 E tsF += 24 * 60
  }

  bool acaoEhLigar = !strncmp(ligar, "ON", 2);

  int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool estaNoIntervalo = (tsAgora >= tsI && tsAgora <= tsF);
  if (!acaoEhLigar) estaNoIntervalo = !estaNoIntervalo;

  // Serial.printf("tsI=%d\ntsF=%d\ntsAgora=%d\n", tsI, tsF, tsAgora);
  // if (estaNoIntervalo) Serial.printf("LIGAR [%s]\n", nomeReles[num]);
  // else                 Serial.printf("DESLIGAR [%s]\n", nomeReles[num]);

  return controlaRele(num + 1, estaNoIntervalo);
}

void onChangeMinute(tm timeinfo) {
  for (int r=0; r < 8; r++) {
    String msg = checkRegra(r, timeinfo);
    if (msg != "") {
      Serial.printf("[%d:%d] > %s\n", timeinfo.tm_hour, timeinfo.tm_min, msg.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);
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
