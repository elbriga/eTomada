#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "loga.h"

#define MUTEX_TIMEOUT 2500

extern SemaphoreHandle_t configMutex;
extern SemaphoreHandle_t modeloMutex;

void mutexInit();

class MutexLock
{
public:
    MutexLock(SemaphoreHandle_t mutex, const char *quem) : mutex(mutex), locked(false)
    {
        // loga("LOCK", LOG_DEBUG, "## Lock [%s] ##", quem);
        locked = xSemaphoreTake(mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT));
        if (!locked)
            loga("LOCK", LOG_CRITICO, "Erro de Lock em [%s]", quem);
    }

    ~MutexLock()
    {
        if (locked)
        {
            xSemaphoreGive(mutex);
        }
    }

    operator bool() const
    {
        return locked;
    }

private:
    SemaphoreHandle_t mutex;
    bool locked;
};
