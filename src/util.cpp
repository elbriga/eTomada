#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UTIL", nivel, fmt, ##__VA_ARGS__)

void utilDIE(const char *msg)
{
  logaM(LOG_CRITICO, ">>> DIE!!! [%s]", msg);
  ESP.restart();
}

bool utilEspSuportaOTA()
{
  int tamanhoFlash = ESP.getFlashChipSize() / (1024 * 1024);

  const esp_partition_t *ota1 =
      esp_partition_find_first(
          ESP_PARTITION_TYPE_APP,
          ESP_PARTITION_SUBTYPE_APP_OTA_1,
          NULL);

  return (tamanhoFlash >= 8) && (ota1 != nullptr);
}

int utilVersionToInt(const char *ver)
{
  int v1, v2, v3;

  sscanf(ver, "%d.%d.%d", &v1, &v2, &v3);

  return v1 * 1000000 + v2 * 1000 + v3;
}

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino)
{
  File origem = LittleFS.open(pathOrigem, "r");
  if (!origem)
  {
    logaM(LOG_AVISO, "ERRO: utilCopiaArquivo [%s] nao encontrado", pathOrigem);
    return 1;
  }

  File destino = LittleFS.open(pathDestino, "w");

  if (!destino)
  {
    origem.close();
    logaM(LOG_CRITICO, "ERRO: utilCopiaArquivo impossivel criar [%s]", pathDestino);
    return 2;
  }

  int bytesCopiados = 0;
  uint8_t buffer[256];
  while (origem.available())
  {
    size_t lidos = origem.read(buffer, sizeof(buffer));
    if (lidos > 0)
    {
      size_t gravados = destino.write(buffer, lidos);
      bytesCopiados += gravados;
    }
  }

  origem.close();
  destino.close();

  logaM(LOG_NORMAL, "Arquivo [%s] copiado para [%s] - %d bytes", pathOrigem, pathDestino, bytesCopiados);

  return 0;
}

const char *utilGetDiaSemana(struct tm timeinfo)
{
  switch (timeinfo.tm_wday)
  {
  case 0:
    return "Dom";
  case 1:
    return "Seg";
  case 2:
    return "Ter";
  case 3:
    return "Qua";
  case 4:
    return "Qui";
  case 5:
    return "Sex";
  case 6:
    return "Sab";
  default:
    return "---";
  }
}
