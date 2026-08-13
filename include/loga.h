#pragma once
#include <Arduino.h>
#include <stdarg.h>

enum LogLevel
{
    LOG_DESATIVADO = 0,
    LOG_CRITICO = 1,
    LOG_AVISO = 5,
    LOG_NORMAL = 10,
    LOG_DEBUG0 = 50,
    LOG_DEBUG = 70,
    LOG_TESTE = 100,
};

void logaInit();
bool logaRemotoAtivo();
String logaGetLogServer();

// Funções novas
void loga(const char *modulo, LogLevel nivel, const char *fmt, ...);
void logaV(const char *modulo, LogLevel nivel, const char *fmt, va_list args);

void logaTitulo(const char *msg);
