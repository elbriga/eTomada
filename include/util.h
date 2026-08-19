#pragma once

void utilDIE(const char *msg);

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino);

const char *utilGetDiaSemana(struct tm timeinfo);

int utilVersionToInt(const char *ver);
