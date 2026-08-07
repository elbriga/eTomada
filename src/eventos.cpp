#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "eventos.h"
#include "recurso.h"
#include "mestre.h"
#include "regras.h"
#include "loga.h"

#define EVENTOS_TAMNHO_FILA 20

QueueHandle_t filaEventos;
void eventosProcessaTask(void *);

void eventosInit()
{
    filaEventos = xQueueCreate(EVENTOS_TAMNHO_FILA, sizeof(Evento));
    if (!filaEventos)
    {
        logaMensagem(">>>>>> ERRO filaEventos!!!!");
        // TODO :: DIE!
        return;
    }

    xTaskCreatePinnedToCore(
        eventosProcessaTask,
        "filaDeEventos",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void eventoPost(Evento evento)
{
    xQueueSend(filaEventos, &evento, 0);
}

void eventosProcessaTask(void *)
{
    Evento evento;

    while (true)
    {
        if (xQueueReceive(filaEventos, &evento, portMAX_DELAY))
        {
            Recurso *rec = evento.recurso;
            bool processaRegras = true;
            bool atualiza = true;

            switch (evento.tipo)
            {
            case EVENTO_VALOR_MUDOU:

                break;
            }

            if (processaRegras)
                regrasProcessaEvento(evento);

            if (atualiza)
            {
                recursoEnviaSSE(rec);
                mestreEnviaEvento(rec);
            }
        }
    }
}
