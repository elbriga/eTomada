#include "mutex.h"

// Lock para mudanças de configuração no prefs e ReInit() do modelo
SemaphoreHandle_t configMutex = NULL;
// Lock do modelo
SemaphoreHandle_t modeloMutex = NULL;

void mutexInit()
{
    configMutex = xSemaphoreCreateMutex();
    modeloMutex = xSemaphoreCreateMutex();
}
