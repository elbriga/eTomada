#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t releMutex;
extern SemaphoreHandle_t prefsMutex;

void mutexInit();

class MutexLock {
public:
    MutexLock(SemaphoreHandle_t mutex,
              TickType_t timeout = portMAX_DELAY)
        : mutex(mutex), locked(false)
    {
        locked = xSemaphoreTake(mutex, timeout);
    }

    ~MutexLock()
    {
        if (locked) {
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
