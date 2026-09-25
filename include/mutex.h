#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t recursosMutex;
extern SemaphoreHandle_t prefsMutex;

void mutexInit();

class MutexLock
{
public:
    MutexLock(SemaphoreHandle_t mutex) : mutex(mutex), locked(false)
    {
        locked = xSemaphoreTake(mutex, pdMS_TO_TICKS(2500));
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
