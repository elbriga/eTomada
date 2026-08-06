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

void anunciadorPost(TipoAnuncio tipo, Recurso *recurso)
{
    Anuncio a = {
        .tipo = tipo,
        .recurso = recurso};

    xQueueSend(filaAnuncios, &a, 0);
}

void anunciadorTask(void *)
{
    Anuncio a;

    while (true)
    {
        if (xQueueReceive(filaAnuncios, &a, portMAX_DELAY))
        {
            switch (a.tipo)
            {
            case ANUNCIO_RECURSO:
                logaMensagem("ANUNCIO_RECURSO <<<<<<<<<<<<<<<<");
                recursoEnviaSSE(a.recurso);
                mestreEnviaEvento(a.recurso);
                break;
            }
        }
    }
}
