#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include "eTomada.h"
#include "loga.h"
#include "hardwareProfile.h"
#include "util.h"
#include "wifi.h"
#include "shaCache.h"

#define OTA_SERVER "10.0.0.1" // TODO :: Unificar em eTomadaServer junto com o log-server
#define OTA_TAMANHO_MINIMO_FLASH 4

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("OTA", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static bool downloadFirmwareNovo = true;
static bool downloadWWWNovo = true;

static int otaCheckDelayInicial = 5000;
static bool otaSupported = false;

void otaUpdateTask(void *args);
bool otaEspSuportaOTA();
bool otaChecaNovoFirmware(bool ehBoot);
bool otaChecaBinario(JsonDocument &doc, bool ehBoot);
bool otaDownload(const char *url);
bool otaChecaWWW(JsonDocument &doc);
bool otaDownloadWWW(const char *path);
const char *otaGetState();

void otaInit()
{
    // TODO :: 60s
    otaCheckDelayInicial = 5 * 1000;

    otaSupported = otaEspSuportaOTA();
    if (otaSupported)
    {
        logaM(LOG_NORMAL, "Estado OTA: %s", otaGetState());

        if (!WiFiGetModoAP())
        {
            logaM(LOG_NORMAL, "Verificando novo firmware:");
            if (!otaChecaNovoFirmware(true))
            {
                logaM(LOG_AVISO, "Falha na checagem de firmware. Tentar de novo em 10 minutos");
                otaCheckDelayInicial = 10 * 60 * 1000;
            }
        }
    }

    if (!WiFiGetModoAP())
    {
        // Desligado!
        // Inicializar a Task que vai conferir o firmware e os arquivos estaticos
        // xTaskCreate(
        //     otaUpdateTask,
        //     "otaUpdate",
        //     8192,
        //     nullptr,
        //     1,
        //     nullptr);
    }
}

void otaUpdateTask(void *args)
{
    vTaskDelay(pdMS_TO_TICKS(otaCheckDelayInicial));

    while (1)
    {
        if (!otaChecaNovoFirmware(false))
        {
            logaM(LOG_AVISO, "Falha na checagem de firmware. Tentar de novo em 10 minutos");
            vTaskDelay(pdMS_TO_TICKS(9 * 60 * 1000));
        }

        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }

    // Nao chega aqui!
    vTaskDelete(NULL);
}

bool otaChecaNovoFirmware(bool ehBoot)
{
    String url = "http://" + String(OTA_SERVER) + "/firmware/eTomada.json";

    logaM(LOG_DEBUG0, "OTA: Verificando %s", url.c_str());

    HTTPClient http;
    http.setTimeout(1000);
    http.setConnectTimeout(1000);

    http.begin(url);

    esp_task_wdt_reset(); // alimenta o watchdog
    int code = http.GET();
    esp_task_wdt_reset(); // alimenta o watchdog

    if (code != 200)
    {
        logaM(LOG_AVISO, "Falha no Download : [%d]", code);
        return true;
    }

    JsonDocument doc;
    if (utilLeJson("otaChecaNovoFirmware", doc, http.getStream()))
    {
        logaM(LOG_AVISO, "Falha JSON ao contactar servidor de Firmware");
        return false;
    }

    bool retBin = true;
    if (otaSupported)
        retBin = otaChecaBinario(doc, ehBoot);

    bool retWWW = true;
    if (!ehBoot)
        retWWW = otaChecaWWW(doc);

    doc.clear();

    return retBin && retWWW;
}

bool otaChecaWWW(JsonDocument &doc)
{
    JsonObject www = doc["www"]["arquivos"].as<JsonObject>();
    for (JsonPair arq : www)
    {
        String pathStr = String("/www") + arq.key().c_str();
        const char *path = pathStr.c_str();
        const char *remoteSha = arq.value().as<const char *>();

        if (!LittleFS.exists(path))
        {
            logaM(LOG_AVISO, "NOVO arquivo www: [%s]", path);
            otaDownloadWWW(path);
            continue;
        }

        // calcular SHA256 do arquivo local
        const char *localSha = shaGet(path);
        if (!localSha)
        {
            logaM(LOG_CRITICO, "ERRO SHA - ABORTANDO arquivo www: [%s]", path);
            // NÃO baixar arquivo??
            continue;
        }

        // comparar com remoteSha
        if (strcmp(localSha, remoteSha) != 0)
        {
            logaM(LOG_AVISO, "Nova Versao arquivo www: [%s]", path);
            otaDownloadWWW(path);
            continue;
        }

        // logaM(LOG_DEBUG, ">> [%s] Atualizado!", path);
    }
    return true;
}

bool otaDownloadWWW(const char *path)
{
    const char *tempPath = "/tempFile.tmp";
    String url = "http://" + String(OTA_SERVER) + "/firmware" + String(path);

    logaM(LOG_AVISO, "BAIXAR [%s]", url.c_str());

    HTTPClient http;
    http.setTimeout(1000);
    http.setConnectTimeout(1000);

    if (!http.begin(url))
    {
        logaM(LOG_AVISO, "otaDownloadWWW :: Erro iniciando HTTP");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        logaM(LOG_AVISO, "otaDownloadWWW :: Erro HTTP [%d]", httpCode);
        http.end();
        return false;
    }

    int total = http.getSize();

    logaM(LOG_NORMAL, "Baixando %d bytes", total);

    File file = LittleFS.open(tempPath, "w");
    if (!file)
    {
        logaM(LOG_AVISO, "otaDownloadWWW :: Erro abrindo [%s] para [%s]", tempPath, path);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();

    uint8_t buffer[1024];
    size_t totalLido = 0;

    while (http.connected() && (total < 0 || totalLido < (size_t)total))
    {
        size_t disponivel = stream->available();

        if (disponivel)
        {
            size_t lidos = stream->readBytes(
                buffer,
                min(disponivel, sizeof(buffer)));

            if (lidos == 0)
                break;

            size_t gravados = file.write(buffer, lidos);

            if (gravados != lidos)
            {
                logaM(LOG_AVISO, "otaDownloadWWW :: Erro escrevendo no LittleFS");
                file.close();
                http.end();
                return false;
            }

            totalLido += lidos;
        }
        else
        {
            vTaskDelay(1);
        }
    }

    file.close();
    http.end();

    if (total >= 0 && totalLido != (size_t)total)
    {
        logaM(LOG_AVISO, "otaDownloadWWW :: Download incompleto: %d/%d bytes", totalLido, total);
        return false;
    }

    // Renomear o tempFile para o arquivo final
    if (!LittleFS.rename(tempPath, path))
    {
        logaM(LOG_CRITICO, "otaDownloadWWW :: Erro no rename!");
        return false;
    }

    shaRemoveCache(path);

    logaM(LOG_NORMAL, "Download concluído: %d bytes", totalLido);
    return true;
}

bool otaChecaBinario(JsonDocument &doc, bool ehBoot)
{
    int minhaVersao = utilVersionToInt(eTomadaGetVersao().c_str());

    String versaoServerStr = "";
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

        if (ehBoot) // opção enviada no boot, para fazer o download antes de começar a funcionar
        {
            if (otaDownload(urlFW.c_str()))
            {
                logaM(LOG_AVISO, "Reiniciando para novo firmware...");
                utilRestart("Atualizacao de Firmware", true);
            }
            else
            {
                // Desativar OTA visto que o download falhou
                logaM(LOG_CRITICO, "Falha no download do firmware...");
                return false;
            }
        }
        else
        {
            logaM(LOG_AVISO, "Reiniciando para fazer o download de [%s]", urlFW.c_str());
            utilRestart("Restart para iniciar download de firmware", true);
        }
    }
    else if (versaoServer == minhaVersao)
        logaM(ehBoot ? LOG_NORMAL : LOG_DEBUG, "Estamos na ultima versao");
    else
        logaM(ehBoot ? LOG_NORMAL : LOG_DEBUG, "Estamos em versão DEV!! Versão Server: [%s]", versaoServerStr.c_str());

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
    int dbm = WiFi.RSSI();
    if (dbm <= -70)
    {
        logaM(LOG_CRITICO, "ABORTANDO download do firmware. WiFi muito fraco [%d]!", dbm);
        return false;
    }

    HTTPClient http;
    http.setTimeout(1000);
    http.setConnectTimeout(1000);

    logaM(LOG_NORMAL, "Baixando firmware: %s", url);

    if (!http.begin(url))
    {
        logaM(LOG_CRITICO, "Falha ao iniciar HTTP");
        return false;
    }

    int httpCode = http.GET();
    esp_task_wdt_reset(); // alimenta o watchdog

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
    int wdtCounter = 0;

    while (http.connected() && totalRecebido < total)
    {
        if (wdtCounter++ >= 250) // 250 ticks
        {
            wdtCounter = 0;
            esp_task_wdt_reset(); // alimenta o watchdog
        }

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
            vTaskDelay(1);
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

// ============================================================
// OTA via HTTP Upload
// ============================================================

static const char *otaUploadErro = nullptr;
static size_t otaUploadTamanhoEsperado = 0;
static size_t otaUploadTamanhoAtual = 0;
static int otaUploadUltimoPercentual = -1;

void otaUpload(
    AsyncWebServerRequest *request,
    String filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool final)
{
    // Primeiro chunk
    if (index == 0)
    {
        otaUploadErro = nullptr;
        otaUploadTamanhoAtual = 0;
        otaUploadUltimoPercentual = -1;

        if (!request->hasParam("tamanho"))
        {
            logaM(LOG_AVISO, "OTA: parametro tamanho ausente");
            otaUploadErro = "parametro tamanho ausente";
            return;
        }

        otaUploadTamanhoEsperado = request->getParam("tamanho")->value().toInt();

        logaM(LOG_NORMAL,
              "OTA upload iniciando: %s (%u bytes)",
              filename.c_str(),
              (unsigned)otaUploadTamanhoEsperado);

        if (otaUploadTamanhoEsperado == 0)
        {
            otaUploadErro = "tamanho invalido";
            return;
        }

        if (!Update.begin(otaUploadTamanhoEsperado))
        {
            otaUploadErro = "Update.begin falhou";

            logaM(LOG_CRITICO,
                  "OTA: Update.begin falhou: %s",
                  Update.errorString());

            return;
        }
    }

    // Recebendo dados
    if (otaUploadErro == nullptr && len > 0)
    {
        size_t gravado = Update.write(data, len);

        if (gravado != len)
        {
            otaUploadErro = "erro ao gravar";

            logaM(LOG_CRITICO,
                  "OTA: erro ao gravar: %s",
                  Update.errorString());

            Update.abort();
            return;
        }

        otaUploadTamanhoAtual += gravado;

        int percentual =
            (otaUploadTamanhoAtual * 100) / otaUploadTamanhoEsperado;

        if (percentual / 10 != otaUploadUltimoPercentual / 10)
        {
            otaUploadUltimoPercentual = percentual;

            logaM(LOG_NORMAL,
                  "OTA: %d%% (%u/%u bytes)",
                  percentual,
                  (unsigned)otaUploadTamanhoAtual,
                  (unsigned)otaUploadTamanhoEsperado);
        }
    }

    // Upload terminou
    if (final)
    {
        if (otaUploadErro != nullptr)
            return;

        logaM(LOG_AVISO,
              "OTA recebido: %u bytes",
              (unsigned)otaUploadTamanhoAtual);

        if (otaUploadTamanhoAtual != otaUploadTamanhoEsperado)
        {
            logaM(LOG_CRITICO,
                  "OTA: esperado=%u recebido=%u",
                  (unsigned)otaUploadTamanhoEsperado,
                  (unsigned)otaUploadTamanhoAtual);

            otaUploadErro = "tamanho recebido diferente";
            Update.abort();
            return;
        }

        if (!Update.end(true))
        {
            otaUploadErro = "Update.end falhou";

            logaM(LOG_CRITICO,
                  "OTA: Update.end falhou: %s",
                  Update.errorString());

            return;
        }

        if (!Update.isFinished())
        {
            otaUploadErro = "OTA nao finalizado";
            logaM(LOG_CRITICO, "OTA: nao finalizado");
            return;
        }

        logaM(LOG_AVISO, "OTA upload concluido!");
    }
}

void otaUploadHelper(AsyncWebServerRequest *request)
{
    if (otaUploadErro != nullptr)
    {
        String resposta = "{\"ok\":false,\"msg\":\"";
        resposta += otaUploadErro;
        resposta += "\"}";

        request->send(400, "application/json", resposta);
        return;
    }

    request->send(
        200,
        "application/json",
        R"({"ok":true,"msg":"ota ok > restart"})");

    utilRestart("OTA upload");
}
