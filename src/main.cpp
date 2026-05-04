#include <Arduino.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <Preferences.h>

// WiFi credentials.
const char* WIFI_SSID = "Ligga Gabriel 2.4g";
const char* WIFI_PASS = "09876543";
IPAddress ipStr;
// Set web server port number to 80
WiFiServer server(80);

// TODO :: Struct rele
// Reles 1 a 8
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
int timeoutMsg = 0;
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
    delay(500);
  }
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
    sprintf(msg, "%s %s    (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"), nomeReles[r].c_str(), numRele, pinosReles[r]);
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
      mostraMsg(msg, 5000);
    }
  }
}

void handleClient(WiFiClient client, struct tm timeinfo) {
  unsigned long currentTime = millis();
  unsigned long previousTime = currentTime; 
  // Define timeout time in milliseconds (example: 2000ms = 2s)
  const long timeoutTime = 5000;

  Serial.println("New Client.");

  String currentLine = "";                // make a String to hold incoming data from the client
  String header = "";
  while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
    currentTime = millis();
    if (client.available()) {             // if there's bytes to read from the client,
      char c = client.read();             // read a byte, then
      Serial.write(c);                    // print it out the serial monitor
      header += c;
      if (c == '\n') {                    // if the byte is a newline character
        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0) {
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          
          /* turns the GPIOs on and off
          if (header.indexOf("GET /26/on") >= 0) {
            Serial.println("GPIO 26 on");
            output26State = "on";
            digitalWrite(output26, HIGH);
          } else if (header.indexOf("GET /26/off") >= 0) {
            Serial.println("GPIO 26 off");
            output26State = "off";
            digitalWrite(output26, LOW);
          } else if (header.indexOf("GET /27/on") >= 0) {
            Serial.println("GPIO 27 on");
            output27State = "on";
            digitalWrite(output27, HIGH);
          } else if (header.indexOf("GET /27/off") >= 0) {
            Serial.println("GPIO 27 off");
            output27State = "off";
            digitalWrite(output27, LOW);
          }*/
          
          // Display the HTML web page
          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          // CSS to style the on/off buttons 
          // Feel free to change the background-color and font-size attributes to fit your preferences
          client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
          client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
          client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
          client.println(".button2 {background-color: #555555;}</style></head>");

          char formattedTime[32];
          strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
          
          // Web Page Heading
          client.println(String("<body><h1>eTomada!</h1><br>") + String(formattedTime) + "<br><br>Reles:<br>");
          
          for (int r=0; r < 8; r++) {
            if (regraReles[r] == "") continue;
            client.println("<p>RELE " + String(r+1) + ": " + nomeReles[r] + " > " + regraReles[r] + " = " +
                (estadoReles[r] ? "Ligado" : "Desligado") + "</p>");
          }

          /* Display current state, and ON/OFF buttons for GPIO 26  
          // If the output26State is off, it displays the ON button       
          if (output26State=="off") {
            client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
          } else {
            client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
          } 
              
          // Display current state, and ON/OFF buttons for GPIO 27  
          client.println("<p>GPIO 27 - State " + output27State + "</p>");
          // If the output27State is off, it displays the ON button       
          if (output27State=="off") {
            client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
          } else {
            client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
          }*/

          client.println("</body></html>");
          
          // The HTTP response ends with another blank line
          client.println();
          // Break out of the while loop
          break;
        } else { // if you got a newline, then clear currentLine
          currentLine = "";
        }
      } else if (c != '\r') {  // if you got anything else but a carriage return character,
        currentLine += c;      // add it to the end of the currentLine
      }
    }
  }

  // Close the connection
  client.stop();
  Serial.println("Client disconnected.");
  Serial.println("");
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
      delay(5000);
    }
    delay(500);
  }
  Serial.println("OK!");

  ipStr = WiFi.localIP();
  Serial.print("Endereço IP: ");
  Serial.println(ipStr);
  
  Serial.print("NTP: ");
  tft.drawString(0, 40, "Buscando Hora...");
  tft.display();
  syncTime();
  Serial.println("OK!");
  
  server.begin();
  delay(100);

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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    Serial.println("Failed to obtain time");
    //return NULL;
  }

  if (timeinfo.tm_sec != lastSecond && timeoutMsg < millis()) {
    lastSecond = timeinfo.tm_sec;
    timeoutMsg = 0;

    // Atualizar o relogio
    char formattedTime[10];
    char msgDataHora[32];
    //strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
    sprintf(msgDataHora, "  %s    %s", getDiaSemana(timeinfo).c_str(), formattedTime);
    mostraMsg(msgDataHora);
  }

  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;
    onChangeMinute(timeinfo);

    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
      // Ao mudar de dia - sync NTP de novo
      // TODO
    }
  }

  delay(1);

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    handleClient(client, timeinfo);
  }
}
