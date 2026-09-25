#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

#include "recovery.h"
#include "ota.h"
#include "hardwareProfile.h"
#include "util.h"

// ============================================================
// Config
// ============================================================

#define RECOVERY_BOOT_COUNT 3
#define RECOVERY_BOOT_TIMEOUT 25000

#define RECOVERY_WIFI_TIMEOUT_MS 15000

#define RECOVERY_WIFI_SSID "GLS"
#define RECOVERY_WIFI_PASS "Lola09876543*"

#define RECOVERY_AP_PREFIX "eTomada-Recovery-"
#define RECOVERY_AP_PASS "09876543"

#define RECOVERY_STORAGE_NAMESPACE "recovery"
#define RECOVERY_STORAGE_KEY_BOOTS "boots"

// ============================================================
// Externos
// ============================================================

extern AsyncWebServer httpServer;
extern const HardwareProfile hardwareProfile;

// ============================================================
// Estado
// ============================================================

static bool recoveryAtivo = false;

static bool bootAguardandoOK = false;
static uint32_t bootInicio = 0;

static bool modoAP = false;
static String apSSID;

static bool ledUltimoEstado = false;

bool recoveryGetAtivo()
{
    return recoveryAtivo;
}

// ============================================================
// Storage
// ============================================================

static bool recoveryStorageRead(uint8_t &boots)
{
    Preferences prefs;

    if (!prefs.begin(RECOVERY_STORAGE_NAMESPACE, false))
        return false;

    boots = prefs.getUChar(RECOVERY_STORAGE_KEY_BOOTS, 0);

    prefs.end();

    return true;
}

static bool recoveryStorageWrite(uint8_t boots)
{
    Preferences prefs;

    if (!prefs.begin(RECOVERY_STORAGE_NAMESPACE, false))
        return false;

    bool ok = prefs.putUChar(RECOVERY_STORAGE_KEY_BOOTS, boots) == sizeof(boots);

    prefs.end();

    return ok;
}

// ============================================================
// Boot recovery
// ============================================================

bool recoveryBoot()
{
    uint8_t boots;

    if (!recoveryStorageRead(boots))
    {
        Serial.println("ERRO lendo contador de recovery");
        return false;
    }

    if (boots < 255)
        boots++;

    Serial.printf("Recovery boot: %u/%u\n",
                  boots,
                  RECOVERY_BOOT_COUNT);

    if (boots >= RECOVERY_BOOT_COUNT)
    {
        Serial.println("Entrando em modo recovery");

        // Encerra a sequência.
        //
        // Depois de OTA/reboot, o próximo boot começa novamente em 1.
        if (!recoveryStorageWrite(0))
            Serial.println("ERRO zerando contador de recovery");

        recoveryAtivo = true;
        return true;
    }

    if (!recoveryStorageWrite(boots))
    {
        Serial.println("ERRO gravando contador de recovery");
        return false;
    }

    bootAguardandoOK = true;
    bootInicio = millis();

    return false;
}

void recoveryBootTick()
{
    if (!bootAguardandoOK)
        return;

    if (millis() - bootInicio < RECOVERY_BOOT_TIMEOUT)
        return;

    if (!recoveryStorageWrite(0))
    {
        Serial.println("ERRO confirmando boot. Tentar de novo em 10 segundos");

        bootInicio = millis();
        return;
    }

    Serial.println("Boot confirmado");

    bootAguardandoOK = false;
}

// ============================================================
// LED
// ============================================================

static void recoveryLedLoop()
{
    if (hardwareProfile.ledPin == 255)
        return;

    // Pisca rápido em recovery: 100 ms
    bool estado = (millis() / 100) % 2;

    if (estado == ledUltimoEstado)
        return;

    ledUltimoEstado = estado;

    digitalWrite(
        hardwareProfile.ledPin,
        hardwareProfile.ledInvertido ? !estado : estado);
}

// ============================================================
// Device ID
// ============================================================

static String recoveryDeviceID()
{
    uint64_t mac = ESP.getEfuseMac();

    char id[7];
    snprintf(id, sizeof(id), "%06llX", mac & 0xFFFFFFULL);

    return String(id);
}

// ============================================================
// WiFi
// ============================================================

static void recoveryWifiInit()
{
    // --------------------------------------------------------
    // Tenta primeiro WiFi fixo do recovery
    // --------------------------------------------------------

    WiFi.mode(WIFI_STA);

    WiFi.begin(RECOVERY_WIFI_SSID, RECOVERY_WIFI_PASS);

    Serial.printf("Recovery conectando em %s", RECOVERY_WIFI_SSID);

    uint32_t inicio = millis();
    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - inicio < RECOVERY_WIFI_TIMEOUT_MS)
    {
        // recoveryLedLoop(); // manter acesso durante o boot
        Serial.print(".");
        delay(50);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();

        Serial.print("Recovery WiFi conectado. IP: ");
        Serial.println(WiFi.localIP());

        modoAP = false;

        return;
    }

    Serial.println();
    Serial.println("Recovery: falha no WiFi");

    // --------------------------------------------------------
    // Fallback AP
    // --------------------------------------------------------

    Serial.println("Iniciando AP de recovery");

    WiFi.disconnect(true);

    delay(100);

    WiFi.mode(WIFI_AP);

    apSSID = String(RECOVERY_AP_PREFIX) + recoveryDeviceID();

    if (!WiFi.softAP(apSSID.c_str(), RECOVERY_AP_PASS))
    {
        Serial.println("ERRO iniciando AP recovery");
        return;
    }

    modoAP = true;

    Serial.printf("Recovery AP: %s\n", apSSID.c_str());
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
}

// ============================================================
// API comum APP / Recovery
// ============================================================

void recoveryAPIRegister()
{
    httpServer.on("/api/ota", HTTP_POST, otaUploadHelper, otaUpload);

    httpServer.on("/api/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
                  {
    request->send(200, "application/json", R"({"msg":"OK - vou reiniciar"})");

    utilRestart("rAPI!"); });
}

// ============================================================
// HTTP recovery
// ============================================================

static void recoveryHttpInit()
{
    httpServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
                  {
                      Serial.println("GET /api/status");

                      IPAddress ip = modoAP ? WiFi.softAPIP() : WiFi.localIP();
                      String ssid = modoAP ? apSSID : WiFi.SSID();

                      String resposta;
                      resposta.reserve(192);

                      resposta = F("{\"mode\":\"recovery\",\"ssid\":\"");

                      resposta += ssid;

                      resposta += F("\",\"ip\":\"");
                      resposta += ip.toString();

                      resposta += F("\",\"rssi\":");
                      resposta += modoAP ? 0 : WiFi.RSSI();

                      resposta += F(",\"uptime\":");
                      resposta += millis();

                      resposta += "}";

                      request->send(200, "application/json", resposta); });

    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  {
                    Serial.println("GET /");
                    request->send(200, "text/plain", "eTomada Recovery"); });

    recoveryAPIRegister();

    httpServer.begin();

    Serial.println("HTTP Recovery iniciado");
}

// ============================================================
// Init / Loop
// ============================================================

void recoveryInit()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("       eTomada Recovery");
    Serial.println("==============================");

    // --------------------------------------------------------
    // Hardware seguro
    // --------------------------------------------------------

    if (hardwareProfile.ledPin != 255)
    {
        pinMode(hardwareProfile.ledPin, OUTPUT);
        // LED aceso até conectar ao WiFi > loop
        digitalWrite(hardwareProfile.ledPin, !hardwareProfile.ledInvertido);
    }

    // Por segurança, recovery sempre inicia com os reles desligados
    /* TODO
    if (hardwareProfile.relePin != 255)
    {
        pinMode(hardwareProfile.relePin, OUTPUT);
        digitalWrite(hardwareProfile.relePin, LOW);
    }*/

    recoveryWifiInit();

    recoveryHttpInit();
}

static bool doReboot = false;
void recoveryReboot()
{
    doReboot = true;
}

void recoveryLoop()
{
    recoveryLedLoop();

    vTaskDelay(pdMS_TO_TICKS(50));

    if (doReboot)
    {
        ESP.restart();
        while (1)
        {
        };
    }
}
