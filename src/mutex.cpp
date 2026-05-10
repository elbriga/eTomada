#include "mutex.h"

SemaphoreHandle_t releMutex = NULL;

void mutexInit() {
    releMutex = xSemaphoreCreateMutex();
}
