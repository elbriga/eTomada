#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "anunciador.h"
#include "recurso.h"
#include "mestre.h"
#include "loga.h"

#define ANUNCIADOR_TAMNHO_FILA 20

QueueHandle_t filaAnuncios;
void anunciadorTask(void *);

void anunciadorInit()
{
    filaAnuncios = xQueueCreate(ANUNCIADOR_TAMNHO_FILA, sizeof(Anuncio));
    if (!filaAnuncios)
    {
        logaMensagem(">>>>>> ERRO filaAnuncios!!!!");
    }

    xTaskCreatePinnedToCore(
        anunciadorTask,
        "anunciador",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void anunciadorPost(Anuncio anuncio)
{
    xQueueSend(filaAnuncios, &anuncio, 0);
}

void anunciadorTask(void *)
{
    Anuncio anuncio;

    while (true)
    {
        if (xQueueReceive(filaAnuncios, &anuncio, portMAX_DELAY))
        {
            switch (anuncio.tipo)
            {
            case ANUNCIO_RECURSO:
                recursoEnviaSSE(anuncio.recurso);
                mestreEnviaEvento(anuncio.recurso);
                break;
            }
        }
    }
}
