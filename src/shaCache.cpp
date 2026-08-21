#include <LittleFS.h>
#include <mbedtls/sha256.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("SHA", nivel, fmt, ##__VA_ARGS__)

#define SHA_CACHE_TAMANHO 32
bool shaGenerate(const char *path, char *sha256Hex);

struct ShaCache
{
    char path[32];
    char sha256[65];
    bool valido;
};

ShaCache shaCache[SHA_CACHE_TAMANHO];

void shaInit()
{
    for (int i = 0; i < SHA_CACHE_TAMANHO; i++)
        shaCache[i].valido = false;
}

const char *shaGet(const char *path)
{
    // Procurar no cache
    for (int i = 0; i < SHA_CACHE_TAMANHO; i++)
        if (shaCache[i].valido && !strcmp(shaCache[i].path, path))
            return shaCache[i].sha256;

    // Se nao achou, calcular e guardar
    for (int i = 0; i < SHA_CACHE_TAMANHO; i++)
        if (!shaCache[i].valido)
        {
            if (!shaGenerate(path, shaCache[i].sha256))
            {
                logaM(LOG_CRITICO, "shaGet!!");
                return nullptr;
            }

            strlcpy(shaCache[i].path, path, sizeof(shaCache[i].path));
            shaCache[i].valido = true;

            return shaCache[i].sha256;
        }

    // ERRO!
    logaM(LOG_CRITICO, "shaGet() - Faltou slots?");
    return nullptr;
}

void shaRemoveCache(const char *path)
{
    for (int i = 0; i < SHA_CACHE_TAMANHO; i++)
        if (!strcmp(shaCache[i].path, path))
        {
            shaCache[i].valido = false;
            return;
        }
}

bool shaGenerate(const char *path, char *sha256Hex)
{
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
