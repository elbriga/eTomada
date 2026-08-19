#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "eTomada.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "util.h"

#define OTA_SERVER "192.168.1.220"
#define OTA_TAMANHO_MINIMO_FLASH 4

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("OTA", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

bool otaDownload(const char *url);
const char *otaGetState();

bool otaChecaNovoFirmware(bool fazDownload)
{
    bool downloadFirmwareNovo = true;

    String url = "http://" + String(OTA_SERVER) + "/firmware/eTomada.json";

    logaM(LOG_DEBUG0, "OTA: Verificando %s", url.c_str());

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
            return false;
        }

        String versaoServerStr = "";
        int minhaVersao = utilVersionToInt(eTomadaGetVersao().c_str());

        // Procurar nosso modelo na lista do servidor
        {
            JsonArray devices = doc["devices"];
            for (JsonObject dev : devices)
            {
                if (!strcmp(dev["board"].as<const char *>(), hardwareProfile.board))
                {
                    JsonArray modelos = dev["modelos"];
                    for (JsonObject modelo : modelos)
                    {
                        if (!strcmp(modelo["modelo"].as<const char *>(), hardwareProfile.modelo))
                        {
                            versaoServerStr = modelo["versao"].as<String>();
                        }
                    }
                }
            }
        }

        if (versaoServerStr == "")
        {
            logaM(LOG_AVISO, ">> Nao achei nossa board [%s] modelo [%s] no firmware server!",
                  hardwareProfile.board, hardwareProfile.modelo);
            return true;
        }

        int versaoServer = utilVersionToInt(versaoServerStr.c_str());
        if (versaoServer > minhaVersao)
        {
            logaM(LOG_AVISO, "Novo Firmware!! >> V: %s", versaoServerStr.c_str());

            if (!downloadFirmwareNovo)
            {
                logaM(LOG_AVISO, "ABORTANDO download do novo firmware!");
                return false;
            }

            String urlFW = "http://" + String(OTA_SERVER) + "/firmware/" + String(hardwareProfile.board) + "_" + String(hardwareProfile.modelo) + "_" + versaoServerStr + ".bin";

            if (fazDownload) // opção enviada no boot, para fazer o download antes de começar a funcionar
            {
                if (otaDownload(urlFW.c_str()))
                {
                    logaM(LOG_AVISO, "Reiniciando para novo firmware...");
                    delay(100);
                    ESP.restart();
                }
                else
                {
                    // Desativar OTA visto que o download falhou
                    return false;
                }
            }
            else
            {
                logaM(LOG_AVISO, "Reiniciando para fazer o download de [%s]", urlFW.c_str());
                delay(100);
                ESP.restart();
            }
        }
        else if (versaoServer == minhaVersao)
            logaM(LOG_DEBUG, "Estamos na ultima versao");
        else
            logaM(LOG_DEBUG, "Versão DEV!");
    }
    return true;
}

bool otaEspSuportaOTA()
{
    int tamanhoFlash = ESP.getFlashChipSize() / (1024 * 1024);

    const esp_partition_t *ota1 =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_OTA_1,
            NULL);

    return (tamanhoFlash >= OTA_TAMANHO_MINIMO_FLASH) && (ota1 != nullptr);
}

bool otaDownload(const char *url)
{
    HTTPClient http;

    logaM(LOG_NORMAL, "Baixando firmware: %s", url);

    if (!http.begin(url))
    {
        logaM(LOG_CRITICO, "Falha ao iniciar HTTP");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        logaM(LOG_CRITICO, "HTTP GET falhou: %d", httpCode);
        http.end();
        return false;
    }

    int total = http.getSize();

    logaM(LOG_NORMAL, "Firmware: %d bytes", total);

    if (total <= 0)
    {
        logaM(LOG_CRITICO, "Tamanho do firmware invalido");
        http.end();
        return false;
    }

    if (!Update.begin(total))
    {
        logaM(LOG_CRITICO, "Nao foi possivel iniciar OTA: %s",
              Update.errorString());
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();

    uint8_t buffer[1024];

    size_t totalRecebido = 0;
    int ultimoPercentual = -1;

    while (http.connected() && totalRecebido < total)
    {
        size_t disponivel = stream->available();

        if (disponivel)
        {
            size_t lido = stream->readBytes(
                buffer,
                min(disponivel, sizeof(buffer)));

            if (lido > 0)
            {
                if (Update.write(buffer, lido) != lido)
                {
                    logaM(LOG_CRITICO, "Erro gravando OTA: %s",
                          Update.errorString());

                    Update.abort();
                    http.end();
                    return false;
                }

                totalRecebido += lido;

                int percentual = (totalRecebido * 100) / total;

                // Loga somente a cada 10%
                if (percentual / 10 != ultimoPercentual / 10)
                {
                    ultimoPercentual = percentual;

                    logaM(LOG_NORMAL,
                          "OTA: %d%% (%u/%u bytes)",
                          percentual,
                          (unsigned)totalRecebido,
                          (unsigned)total);
                }
            }
        }
        else
        {
            delay(1);
        }
    }

    logaM(LOG_NORMAL, "Firmware gravado: %u/%d bytes", (unsigned)totalRecebido, total);

    if (totalRecebido != (size_t)total)
    {
        logaM(LOG_CRITICO, "Firmware incompleto");
        Update.abort();
        http.end();
        return false;
    }

    if (!Update.end())
    {
        logaM(LOG_CRITICO, "Falha ao finalizar OTA: %s",
              Update.errorString());
        http.end();
        return false;
    }

    if (!Update.isFinished())
    {
        logaM(LOG_CRITICO, "OTA nao foi finalizado");
        http.end();
        return false;
    }

    logaM(LOG_NORMAL, "OTA concluido com sucesso!");

    http.end();

    return true;
}

const char *otaGetState()
{
    esp_ota_img_states_t estado;

    const esp_partition_t *running =
        esp_ota_get_running_partition();

    if (esp_ota_get_state_partition(running, &estado) == ESP_OK)
    {
        switch (estado)
        {
        case ESP_OTA_IMG_NEW:
            return "NEW";
        case ESP_OTA_IMG_PENDING_VERIFY:
            return "PENDING_VERIFY";
        case ESP_OTA_IMG_VALID:
            return "VALID";
        case ESP_OTA_IMG_INVALID:
            return "INVALID";
        case ESP_OTA_IMG_ABORTED:
            return "ABORTED";
        case ESP_OTA_IMG_UNDEFINED:
            return "UNDEFINED";
        default:
            return "???";
        }
    }

    return "ERR";
}
