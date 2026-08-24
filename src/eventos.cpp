#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "eventos.h"
#include "recurso.h"
#include "mestre.h"
#include "regras.h"
#include "util.h"
#include "loga.h"
#include "rtc-hw.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("EVENTOS", nivel, fmt, ##__VA_ARGS__)

#define EVENTOS_TAMNHO_FILA 20

QueueHandle_t filaEventos;
void eventosProcessaTask(void *);

void eventosInit()
{
    filaEventos = xQueueCreate(EVENTOS_TAMNHO_FILA, sizeof(Evento));
    if (!filaEventos)
        utilDIE(">>>>>> ERRO filaEventos!!!!");

    xTaskCreatePinnedToCore(
        eventosProcessaTask,
        "filaDeEventos",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void eventoPost(TipoEvento tipo,
                Recurso *recurso,
                bool enviaSSE,
                bool enviaMestre)
{
    if (!filaEventos)
    {
        logaM(LOG_CRITICO, "Descartando evento [%d] antes da inicializacao", tipo);
        return;
    }

    Evento evento = {
        .tipo = tipo,
        .recurso = recurso,
        .enviaSSE = enviaSSE,
        .enviaMestre = enviaMestre,
    };
    xQueueSend(filaEventos, &evento, 0);
}

String eventoMockFromJson(uint8_t *json)
{
    JsonDocument doc;
    if (utilLeJson("eventoMockFromJson", doc, json))
        return "JSON Invalido";

    TipoEvento mock = EVENTO_NENHUM;
    String acao = doc["acao"];
    if (acao == "TOGGLE")
        mock = EVENTO_TOGGLE;
    else if (acao == "CLICK")
        mock = EVENTO_CLICK;
    else
    {
        doc.clear();
        return "Acao invalida";
    }

    Recurso *recurso = recursoGet(doc["recursoID"].as<const char *>());

    doc.clear();

    if (!recurso)
        return "Recurso Invalido";
    if (recurso->tipo != RECURSO_BOTAO)
        return "Recurso nao eh botao!";

    eventoPost(mock, recurso, false, true);

    return "OK";
}

void eventosProcessaTask(void *)
{
    Evento evento;

    while (true)
    {
        if (xQueueReceive(filaEventos, &evento, portMAX_DELAY))
        {
            Recurso *recurso = evento.recurso;
            bool processaRegras = true;
            bool atualiza = true;

            switch (evento.tipo)
            {
            case EVENTO_TOGGLE:
                // TODO :: agora o TOGGLE pode vir sozinho da interface: achar outra forma de nao duplicar
                // atualiza = false; // Já será atualizado no EVENTO_ON / EVENTO_OFF, nao duplicar
                break;
            }

            if (processaRegras)
                regrasProcessaEvento(evento);

            if (recurso && atualiza)
            {
                if (evento.enviaSSE)
                    recursoEnviaSSE(recurso);
                if (evento.enviaMestre)
                    mestreEnviaEvento(recurso, evento.tipo);
            }
        }
    }
}
