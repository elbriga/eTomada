#include <Arduino.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UTIL", nivel, fmt, ##__VA_ARGS__)

void utilDIE(const char *msg)
{
  logaM(LOG_CRITICO, ">>> DIE!!! [%s]", msg);
  ESP.restart();
}

void utilRestart(const char *msg)
{
  logaM(LOG_AVISO, ">>> Restart!!! [%s]", msg);

  // TODO :: Salvar estado dos reles

  delay(100);
  ESP.restart();
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

bool utilArquivoSha256(const char *path, char *sha256Hex, size_t hexSize)
{
  if (hexSize < 65)
  {
    logaM(LOG_CRITICO, ">> arquivoSha256 :: buffer muito pequeno!!");
    return false;
  }

  File file = LittleFS.open(path, "r");

  if (!file)
  {
    logaM(LOG_CRITICO, ">> arquivoSha256 :: FNF");
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  if (mbedtls_sha256_starts_ret(&ctx, 0) != 0)
  {
    mbedtls_sha256_free(&ctx);
    file.close();
    logaM(LOG_CRITICO, ">> arquivoSha256 :: erro mbedtls_sha256_starts_ret");
    return false;
  }

  uint8_t buffer[1024];

  while (file.available())
  {
    size_t lidos = file.read(buffer, sizeof(buffer));

    if (lidos == 0)
    {
      mbedtls_sha256_free(&ctx);
      file.close();
      logaM(LOG_CRITICO, ">> arquivoSha256 :: Erro file.read()");
      return false;
    }

    if (mbedtls_sha256_update_ret(&ctx, buffer, lidos) != 0)
    {
      mbedtls_sha256_free(&ctx);
      file.close();
      logaM(LOG_CRITICO, ">> arquivoSha256 :: Erro mbedtls_sha256_update_ret 2");
      return false;
    }
  }

  uint8_t hash[32];

  if (mbedtls_sha256_finish_ret(&ctx, hash) != 0)
  {
    mbedtls_sha256_free(&ctx);
    file.close();
    logaM(LOG_CRITICO, ">> arquivoSha256 :: Erro mbedtls_sha256_finish_ret");
    return false;
  }

  mbedtls_sha256_free(&ctx);
  file.close();

  // Converte os 32 bytes para 64 caracteres hexadecimais
  for (int i = 0; i < 32; i++)
  {
    sprintf(&sha256Hex[i * 2], "%02x", hash[i]);
  }

  sha256Hex[64] = '\0';

  return true;
}