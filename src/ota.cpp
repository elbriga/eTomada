#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>

#include "eTomada.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "util.h"

#define OTA_SERVER "192.168.1.220"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("OTA", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

void otaChecaNovoFirmware()
{
    String url = "http://" + String(OTA_SERVER) + "/firmware/eTomada.json";

    // logaM(LOG_NORMAL, "OTA: Acionando %s", url.c_str());

    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);

    int code = http.GET();

    esp_task_wdt_reset(); // alimenta o watchdog

    if (code == 200)
    {
        // TODO http.getString() é perigoso !!! usar o stream
        // String respBody = http.getString();
        // logaM(LOG_NORMAL, " >> RESP: %s", respBody.c_str());

        JsonDocument doc;
        DeserializationError erro = deserializeJson(doc, http.getStream());
        if (erro)
        {
            logaM(LOG_AVISO, "Falha JSON ao contactar servidor de Firmware");
            return;
        }

        int minhaVersao = utilVersionToInt(eTomadaGetVersao().c_str());

        JsonArray devices = doc["devices"];
        for (JsonObject dev : devices)
        {
            if (!strcmp(dev["hardware"].as<const char *>(), hardwareProfile.board))
            {
                int versaoServer = utilVersionToInt(dev["versao"].as<const char *>());
                if (versaoServer > minhaVersao)
                {
                    logaM(LOG_AVISO, "Novo Firmware!! >> HW: %s - V: %s", dev["hardware"].as<const char *>(), dev["versao"].as<const char *>());
                }
            }
        }
    }
}
