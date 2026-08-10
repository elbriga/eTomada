#include "agendamentos.h"
#include "recurso.h"
#include "loga.h"

#define MAX_ACOES_AGENDADAS 16
AcaoAgendada acoes[MAX_ACOES_AGENDADAS];

void agendamentosProcessaTask(void *);

void agendamentosInit()
{
    memset(acoes, 0, sizeof(acoes));

    xTaskCreatePinnedToCore(
        agendamentosProcessaTask,
        "agendamentos",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void agendamentosAdd(const char *recursoID, bool estado, int timeoutMs)
{
    AcaoAgendada *acao = nullptr;

    // procurar um "slot"
    for (int s = 0; s < MAX_ACOES_AGENDADAS; s++)
    {
        if (!acoes[s].ativa)
        {
            acao = &acoes[s];
            break;
        }
    }

    if (!acao)
    {
        logaMensagem("agendamentosAdd: IMPOSSIVEL ACHAR SLOT!");
        return;
    }

    strlcpy(acao->recursoID, recursoID, sizeof(acao->recursoID));
    acao->estado = estado;

    acao->quando = millis() + timeoutMs;
    acao->ativa = true;
}

void agendamentosProcessaTask(void *)
{
    while (true)
    {
        // procura ações cujo "quando" chegou
        for (int s = 0; s < MAX_ACOES_AGENDADAS; s++)
        {
            AcaoAgendada *acao = &acoes[s];
            if (!acao->ativa)
                continue;

            if ((int32_t)(millis() - acao->quando) >= 0)
            {
                Recurso *r = recursoGet(acao->recursoID);
                if (r)
                    recursoSet(r, acao->estado ? "ON" : "OFF");
                else
                    logaMensagem("agendamentosProcessaTask :: recurso invalido!");

                // remove do slot
                acao->ativa = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
