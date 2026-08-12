#pragma once
#include <stdarg.h>

enum LogLevel
{
    LOG_DESATIVADO = 0,
    LOG_CRITICO = 1,
    LOG_AVISO = 5,
    LOG_NORMAL = 10,
    LOG_TESTE = 15,
    LOG_DEBUG = 20
};

void logaInit();

// Funções novas
void loga(const char *modulo, LogLevel nivel, const char *fmt, ...);
void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args);

// Função legada - TODO :: remover
void logaMensagem(const char *fmt, ...);

void logaTitulo(const char *msg);
