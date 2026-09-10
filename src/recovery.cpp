#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <EEPROM.h>

#include "recovery.h"
#include "config.h"
#include "ota.h"
#include "hardwareProfile.h"
#include "util.h"

// ============================================================
// Config
// ============================================================

#define RECOVERY_WIFI_SSID "GLS"
#define RECOVERY_WIFI_PASS "Lola09876543*"

#define RECOVERY_BOOT_COUNT 3
#define RECOVERY_BOOT_TIMEOUT 10000

#define RECOVERY_WIFI_TIMEOUT_MS 15000

#define RECOVERY_AP_PREFIX "eTomada-Recovery-"
#define RECOVERY_AP_PASS "09876543"

#define RECOVERY_STORAGE_MAGIC 0x52454356UL // "RECV"

// ============================================================
// Externos
// ============================================================

extern WebServer server;
extern const HardwareProfile hardwareProfile;

// ============================================================
// Storage
// ============================================================

#define RECOVERY_STORAGE_MAGIC 0x52454356UL // "RECV"

#if defined(ESP8266)

// ESP8266 usa EEPROM emulada.
// A área do recovery fica separada da Config.

struct RecoveryStorage
{
    uint32_t magic;
    uint8_t boots;
    uint8_t reserved[3];
};

static_assert(
    sizeof(Config) < ETOMADA_LITE_EEPROM_RECOVERY_OFFSET,
    "Config invade area reservada ao recovery");

static_assert(
    ETOMADA_LITE_EEPROM_RECOVERY_OFFSET + sizeof(RecoveryStorage) <=
        ETOMADA_LITE_EEPROM_SIZE,
    "RecoveryStorage nao cabe na EEPROM");

#else

// LN882H:
//
// Flash física: 2 MiB
// User Data:    0x1EC000 - 0x200000
//
// Reservamos o último setor de 4 KiB exclusivamente para
// o contador de boot do recovery.

#define RECOVERY_FLASH_ADDR 0x1FF000
#define RECOVERY_FLASH_SIZE 0x1000

#define RECOVERY_REC_BOOT 0x424F4F54UL // "BOOT"
#define RECOVERY_REC_OK 0x4F4B4F4BUL   // "OKOK"

#endif

// ============================================================
// Estado
// ============================================================

static bool bootAguardandoOK = false;
static uint32_t bootInicio = 0;

static bool modoAP = false;
static String apSSID;

static bool ledUltimoEstado = false;

// ============================================================
// Storage
// ============================================================
static void recoveryStorageRead(RecoveryStorage &storage)
{
    EEPROM.get(
        ETOMADA_LITE_EEPROM_RECOVERY_OFFSET,
        storage);

    if (storage.magic != RECOVERY_STORAGE_MAGIC)
    {
        memset(&storage, 0, sizeof(storage));

        storage.magic = RECOVERY_STORAGE_MAGIC;
    }
}

static bool recoveryStorageWrite(const RecoveryStorage &storage)
{
    EEPROM.put(
        ETOMADA_LITE_EEPROM_RECOVERY_OFFSET,
        storage);

    return EEPROM.commit();
}

// ============================================================
// Boot recovery
// ============================================================

bool recoveryBoot()
{
#if defined(ESP8266)

    EEPROM.begin(ETOMADA_LITE_EEPROM_SIZE);

    RecoveryStorage storage;

    recoveryStorageRead(storage);

    storage.boots++;

    Serial.printf(
        "Recovery boot: %u/%u\n",
        storage.boots,
        RECOVERY_BOOT_COUNT);

    if (storage.boots >= RECOVERY_BOOT_COUNT)
    {
        Serial.println("Entrando em modo recovery");

        // Encerra a sequencia.
        //
        // Depois de OTA/reboot, o próximo boot começa novamente em 1.
        storage.boots = 0;

        if (!recoveryStorageWrite(storage))
            Serial.println("ERRO zerando contador de recovery");

        return true;
    }

    if (!recoveryStorageWrite(storage))
    {
        Serial.println("ERRO gravando contador de recovery");
        return false;
    }

#else

    // Este endereço reservado só é válido para o layout
    // generic-ln882h com flash física de 2 MiB.
    if (Flash.getSize() != 0x200000)
    {
        Serial.printf(
            "Flash inesperada: %u bytes - recovery boot desativado\n",
            Flash.getSize());

        return false;
    }

    uint32_t offset;
    uint8_t boots;

    if (!recoveryStorageScan(offset, boots))
    {
        Serial.println("Inicializando recovery storage");

        if (!Flash.eraseSector(RECOVERY_FLASH_ADDR))
        {
            Serial.println("Erro apagando recovery storage");
            return false;
        }

        boots = 0;
    }

    boots++;

    Serial.printf("Recovery boot: %u/%u\n", boots, RECOVERY_BOOT_COUNT);

    if (!recoveryStorageAppend(RECOVERY_REC_BOOT))
    {
        Serial.println("Erro gravando contador de recovery");
        return false;
    }

    if (boots >= RECOVERY_BOOT_COUNT)
    {
        Serial.println("Entrando em modo recovery");

        // Encerra a sequência.
        // Depois de OTA/reboot, próximo boot começa novamente em 1.
        if (!recoveryStorageAppend(RECOVERY_REC_OK))
            Serial.println("Erro encerrando sequencia de recovery");

        return true;
    }

#endif

    bootAguardandoOK = true;
    bootInicio = millis();

    return false;
}

static bool resetBootsPendente = false;
void recoveryBootTick()
{
    if (!bootAguardandoOK)
        return;

    if (millis() - bootInicio < RECOVERY_BOOT_TIMEOUT)
        return;

#if defined(ESP8266)

    RecoveryStorage storage;

    recoveryStorageRead(storage);

    if (storage.boots != 0 || resetBootsPendente)
    {
        storage.boots = 0;

        if (!recoveryStorageWrite(storage))
        {
            Serial.println("ERRO confirmando boot. Tentar de novo em 10 segundos");
            bootInicio = millis();
            resetBootsPendente = true;
            return;
        }

        resetBootsPendente = false;
        Serial.println("Boot confirmado");
    }
#else
    if (!recoveryStorageAppend(RECOVERY_REC_OK))
    {
        Serial.println("ERRO confirmando boot. Tentar de novo em 10 segundos");
        bootInicio = millis();
        return;
    }

    Serial.println("Boot confirmado");
#endif

    bootAguardandoOK = false;
}

// ============================================================
// LED
// ============================================================

static void recoveryLedLoop()
{
    if (hardwareProfile.ledPin == 255)
        return;

    // Pisca rapido em recovery: 100 ms
    bool estado = (millis() / 100) % 2;

    if (estado == ledUltimoEstado)
        return;

    ledUltimoEstado = estado;

    digitalWrite(
        hardwareProfile.ledPin,
        hardwareProfile.ledInvertido ? !estado : estado);
}

// ============================================================
// WiFi
// ============================================================

static void recoveryWifiInit()
{
    if (config.ssid[0])
    {
        WiFi.mode(WIFI_STA);

        WiFi.hostname(config.deviceID);

        WiFi.begin(config.ssid, config.senha);

        Serial.printf("Recovery conectando em %s", config.ssid);

        uint32_t inicio = millis();
        while (
            WiFi.status() != WL_CONNECTED &&
            millis() - inicio < RECOVERY_WIFI_TIMEOUT_MS)
        {
            // recoveryLedLoop();
            Serial.print(".");
            delay(50);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println();

            Serial.print("Recovery WiFi conectado. IP: ");
            Serial.println(WiFi.localIP());

            return;
        }

        Serial.println();
        Serial.println("Recovery: falha no WiFi");
    }

    // --------------------------------------------------------
    // Fallback AP
    // --------------------------------------------------------

    Serial.println("Iniciando AP de recovery");

    WiFi.disconnect();

    delay(100);

    WiFi.mode(WIFI_AP);

    apSSID =
        String(RECOVERY_AP_PREFIX) +
        String(ESP.getChipId(), HEX);

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
    // Reutiliza exatamente o OTA existente do eTomada Lite.
    server.on(
        "/api/ota",
        HTTP_POST,
        apiOtaFlashHelper,
        apiOtaFlash);

    server.on(
        "/api/reboot",
        HTTP_POST,
        []()
        {
            server.send(
                200,
                "application/json",
                R"({"ok":true,"msg":"reboot"})");

            delay(500);

            ESP.restart();
        });
}

// ============================================================
// HTTP recovery
// ============================================================

static void recoveryHttpInit()
{
    server.on("/api/status", HTTP_GET,
              []()
              {
                  IPAddress ip = modoAP ? WiFi.softAPIP() : WiFi.localIP();
                  char ipStr[16];
                  utilIPToString(ip, ipStr, 16);

                  String ssid = modoAP ? apSSID : WiFi.SSID();

                  String resposta;
                  resposta.reserve(192);

                  resposta = F("{\"mode\":\"recovery\",\"ssid\":\"");
                  resposta += ssid;

                  resposta += F("\",\"ip\":\"");
                  resposta += ipStr;

                  resposta += F("\",\"rssi\":");
                  resposta += modoAP ? 0 : WiFi.RSSI();

                  resposta += F(",\"uptime\":");
                  resposta += millis();

                  resposta += "}";

                  server.send(200, "application/json", resposta);
              });

    server.on("/", HTTP_GET,
              []()
              {
                  server.send(200, "text/plain", "eTomada Lite Recovery");
              });

    recoveryAPIRegister();

    server.begin();

    Serial.println("HTTP Recovery iniciado");
}

// ============================================================
// Init / Loop
// ============================================================

void recoveryInit()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("    eTomada Lite Recovery");
    Serial.println("==============================");

    // O recovery precisa da configuração somente para tentar
    // conectar no WiFi configurado.
    configLoad();

    if (hardwareProfile.ledPin != 255)
    {
        pinMode(hardwareProfile.ledPin, OUTPUT);
        // LED Aceso até conectar no Wifi
        digitalWrite(hardwareProfile.ledPin, !hardwareProfile.ledInvertido);
    }

    // Por seguranca, recovery sempre inicia com rele desligado.
    if (hardwareProfile.relePin != 255)
    {
        pinMode(hardwareProfile.relePin, OUTPUT);
        digitalWrite(hardwareProfile.relePin, LOW);
    }

    recoveryWifiInit();

    recoveryHttpInit();
}

void recoveryLoop()
{
    server.handleClient();

    recoveryLedLoop();

    yield();
}
