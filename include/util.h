#pragma once

void utilDIE(const char *msg);
void utilRestart(const char *msg);

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino);
bool utilArquivoSha256(const char *path, char *sha256Hex, size_t hexSize);

const char *utilGetDiaSemana(struct tm timeinfo);

int utilVersionToInt(const char *ver);
