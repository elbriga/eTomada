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

static bool downloadWWWNovo = true;

static bool otaSupported = false;

bool otaEspSuportaOTA();
bool otaChecaWWW();
bool otaDownloadWWW(const char *path);
const char *otaGetState();

void otaInit()
{
    otaSupported = otaEspSuportaOTA();
    if (otaSupported)
        logaM(LOG_NORMAL, "Estado OTA: %s", otaGetState());
}

bool otaChecaWWW()
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

    doc.clear();

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
