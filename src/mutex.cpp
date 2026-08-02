#include "mutex.h"

SemaphoreHandle_t recursosMutex = NULL;
SemaphoreHandle_t prefsMutex = NULL;

void mutexInit()
{
    recursosMutex = xSemaphoreCreateMutex();
    prefsMutex = xSemaphoreCreateMutex();
}
