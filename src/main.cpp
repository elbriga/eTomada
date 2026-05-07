#include <Arduino.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>

// WiFi credentials.
const char* WIFI_SSID = "Ligga Gabriel 2.4g";
const char* WIFI_PASS = "09876543";
IPAddress ipStr;
// Set web server port number to 80
WiFiServer server(80);

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

// ==============================================================================================
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
    sprintf(msg, "%s %s    (rele %d, pino %d)", (estado ? "Ligando" : "Desligando"), rele->nome.c_str(), numRele, rele->pino);
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

String checkRegra(int num, tm timeinfo) {
  Rele *rele = &reles[num];

  if (rele->regra == "") {
    return "";
  }

  if (rele->regra.length() < 10) {
    // ERRO! Nao deve cair aqui!
    Serial.printf("REGRA RELE %d INVALIDA!!!", num+1);
    return "";
  }

  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligar[3] = {0};
  int lidos = sscanf(rele->regra.c_str(), "%2[^-]-%d:%d-%d:%d", ligar, &hI, &mI, &hF, &mF);
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

void onChangeMinute(struct tm timeinfo) {
  for (int r=0; r < 8; r++) {
    String msg = checkRegra(r, timeinfo);
    if (msg != "") {
      Serial.printf("[%d:%d] > %s\n", timeinfo.tm_hour, timeinfo.tm_min, msg.c_str());
      mostraMsg(msg, 5000);
    }
  }
}


String getDataJSON(struct tm timeinfo) {
  String ret = "{\n";
  
  // Convert struct tm to time_t (Unix timestamp)
  time_t unix_timestamp = mktime(&timeinfo);
  ret += "  \"datahora\": " + String(unix_timestamp) + ",\n";

   char formattedTime[32];
    strftime(formattedTime, sizeof(formattedTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
  ret += "  \"datahorastr\": \"" + String(formattedTime) + "\",\n";

  ret += "  \"reles\": [\n";
  for (int r=0; r < 8; r++) {
    Rele *rele = &reles[r];
    ret += "    {\n";
    ret += "      \"nome\": \"" + rele->nome + "\",\n";
    ret += "      \"regra\": \"" + rele->regra + "\",\n";
    ret += "      \"pino\": " + String(rele->pino) + ",\n";
    ret += "      \"estado\": " + String(rele->estado) + "\n";
    ret += "    } " + String((r < 7) ? "," : "") + "\n";
  }
  ret += "  ]\n";

  return ret + "}";
}

String getQueryStringValue(String qs, String nomeAtr, String def) {
  int pos = qs.indexOf(nomeAtr + "=");
  if (pos < 0) {
    return def;
  }

  int start = pos + nomeAtr.length() + 1;
  int end = qs.indexOf("&", start);
  if (end < 0) end = qs.length();

  return qs.substring(start, end);
}

int getQueryStringValueInt(String qs, String nomeAtr, int def) {
  String intStr = getQueryStringValue(qs, nomeAtr, String(def));
  return intStr.toInt();
}

String setReleConfig(String httpRequest, struct tm timeinfo) {
  int numRele = -1;
  char querystring[128] = {0};
  int lidos = sscanf(httpRequest.c_str(), "GET /setReleConfig/%d?%127s ", &numRele, querystring);
  if (lidos != 2) {
    return "501 Bad Format";
  } else if (numRele < 1 || numRele > 8) {
    return "533 Bad RELE";
  }

  String qs = String(querystring);

  Rele *rele = &reles[numRele - 1];
  rele->nome  = getQueryStringValue(qs, "nome", rele->nome);
  rele->regra = getQueryStringValue(qs, "regra", rele->regra);
  rele->pino  = getQueryStringValueInt(qs, "pino", rele->pino);

  Serial.println("setReleConfig ::");
  Serial.println("RELE " + String(numRele) + " nome:" + rele->nome + " regra:" + rele->regra + " pino:" + String(rele->pino));

  // Setar no prefs
  setPrefsAtr(numRele, "nome", rele->nome);
  setPrefsAtr(numRele, "regra", rele->regra);
  setPrefsAtr(numRele, "pino", String(rele->pino));

  // Checar as regras novamente
  onChangeMinute(timeinfo);

  return "200 OK";
}

void enviaParaClient(WiFiClient client, String msg) {
  Serial.println("httpResponse: " + msg);
  client.println(msg);
}

String getContentType(String path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg"))  return "image/jpeg";
  return "text/plain";
}

void serveFile(WiFiClient &client, String path) {
  if (!FSOK) {
    client.println("HTTP/1.1 505 No FS");
    client.println();
    return;
  }

  File file = LittleFS.open(path, "r");

  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println();
    return;
  }

  enviaParaClient(client, "HTTP/1.1 200 OK");
  enviaParaClient(client, "Content-Type: " + getContentType(path));
  enviaParaClient(client, "Content-Length: " + String(file.size()));
  enviaParaClient(client, "Connection: close");
  enviaParaClient(client, "Access-Control-Allow-Origin: *");
  enviaParaClient(client, "");

  uint8_t buffer[512];

  while (file.available()) {
    size_t len = file.read(buffer, sizeof(buffer));
    client.write(buffer, len);

    esp_task_wdt_reset();
  }

  file.close();
}

void handleClient(WiFiClient client, struct tm timeinfo) {
  unsigned long currentTime = millis();
  unsigned long previousTime = currentTime; 
  // Define timeout time in milliseconds (example: 2000ms = 2s)
  const long timeoutTime = 5000;

  Serial.println("New Client.");

  String currentLine = "";                // make a String to hold incoming data from the client
  String httpRequest = "";
  while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
    currentTime = millis();
    if (client.available()) {             // if there's bytes to read from the client,
      char c = client.read();             // read a byte, then
      Serial.write(c);                    // print it out the serial monitor

      if (c == '\n') {                    // if the byte is a newline character
        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0) {
          String contentType = "text/html";
          String responseBody;
          responseBody.reserve(2048);

          // Verificar o que foi solicitado
          if (httpRequest.indexOf("GET / ") == 0) { // index.html
            serveFile(client, "/index.html");
            break;
          } else if (httpRequest.indexOf("GET /data ") == 0) {
            contentType = "application/json";
            responseBody = getDataJSON(timeinfo);
          } else if (httpRequest.indexOf("GET /setReleConfig/") == 0) {
            // Ler query string
            contentType = "application/json";
            responseBody = setReleConfig(httpRequest, timeinfo);
            if (responseBody != "200 OK") {
              enviaParaClient(client, "HTTP/1.1 " + responseBody);
              enviaParaClient(client, "");
              break;
            }
            responseBody = "{\"response\": \"" + responseBody + "\"}"; // 200 ok!
          } else if (httpRequest.indexOf("OPTIONS") == 0) {
            enviaParaClient(client, "HTTP/1.1 204 No Content");
            enviaParaClient(client, "Access-Control-Allow-Origin: *");
            enviaParaClient(client, "Access-Control-Allow-Methods: GET, PUT, OPTIONS");
            enviaParaClient(client, "Access-Control-Allow-Headers: Content-Type");
            enviaParaClient(client, "");
            return;
          } else {
            // 404
            enviaParaClient(client, "HTTP/1.1 404 Not Found");
            enviaParaClient(client, "Content-Length: 0");
            enviaParaClient(client, "");
            break;
          }

          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          enviaParaClient(client, "HTTP/1.1 200 OK");
          enviaParaClient(client, "Content-Type: " + contentType);
          enviaParaClient(client, "Content-Length: " + String(responseBody.length() + 1)); // + 1 para o \n final
          enviaParaClient(client, "Connection: close");
          enviaParaClient(client, "Access-Control-Allow-Origin: *");
          enviaParaClient(client, "Access-Control-Allow-Methods: GET, PUT, OPTIONS");
          enviaParaClient(client, "Access-Control-Allow-Headers: Content-Type");
          enviaParaClient(client, "");
          enviaParaClient(client, responseBody);
          
          // Break out of the while loop
          break;
        } else { // if you got a newline, then clear currentLine
          if (httpRequest == "")
            httpRequest = currentLine; // Salvar a 1a linha do request
          currentLine = "";
        }
      } else if (c != '\r') {  // if you got anything else but a carriage return character,
        currentLine += c;      // add it to the end of the currentLine
      }
    } else {
      yield();
    }
  }

  // Close the connection
  client.stop();
  Serial.println("Client disconnected.");
  Serial.println("");
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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    Serial.println("Failed to obtain time");
  } else {
    char formattedTime[32];
    strftime(formattedTime, sizeof(formattedTime), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    Serial.println(formattedTime);
  }
  
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
  esp_task_wdt_reset(); // alimenta o watchdog

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
      syncTime();
    }
  }

  delay(1);

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    handleClient(client, timeinfo);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("===");
    Serial.println("WiFi caiu!! Reconectar...");
    Serial.println("===");
    mostraMsg("Reconectando...", 10000);
    WiFiConnect();
  }
}
