#include "mutex.h"

SemaphoreHandle_t releMutex = NULL;
SemaphoreHandle_t prefsMutex = NULL;

void mutexInit() {
    releMutex = xSemaphoreCreateMutex();
    prefsMutex = xSemaphoreCreateMutex();
}
