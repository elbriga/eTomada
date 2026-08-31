#include "agendamentos.h"
#include "recurso.h"
#include "loga.h"
// #include "umidificador.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("AGENDS", nivel, fmt, ##__VA_ARGS__)

struct AcaoAgendada
{
    TipoAgendamento tipo;
    uint32_t quando;

    // Para acao AGEND_RECURSO
    char recursoID[32];
    int estado;
};

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

void agendamentosAdd(TipoAgendamento tipo, int timeoutMs, const char *recursoID, int estado)
{
    // procurar um "slot"
    AcaoAgendada *acao = nullptr;
    for (int s = 0; s < MAX_ACOES_AGENDADAS; s++)
    {
        if (acoes[s].tipo == AGEND_NENHUM)
        {
            acao = &acoes[s];
            break;
        }
    }
    if (!acao)
    {
        logaM(LOG_CRITICO, "agendamentosAdd[%d]: IMPOSSIVEL ACHAR SLOT!", tipo);
        return;
    }

    acao->tipo = tipo;
    acao->quando = millis() + timeoutMs;

    strlcpy(acao->recursoID, recursoID, sizeof(acao->recursoID));
    acao->estado = estado;
}

void agendamentosProcessaTask(void *)
{
    while (true)
    {
        // procura ações cujo "quando" chegou
        for (int s = 0; s < MAX_ACOES_AGENDADAS; s++)
        {
            AcaoAgendada *acao = &acoes[s];
            if (acao->tipo == AGEND_NENHUM)
                continue;

            if ((int32_t)(millis() - acao->quando) >= 0)
            {
                switch (acao->tipo)
                {
                case AGEND_RECURSO:
                {
                    Recurso *r = recursoGet(acao->recursoID);
                    if (!r)
                    {
                        logaM(LOG_CRITICO, "agendamentosProcessaTask :: recurso invalido!");
                        break;
                    }

                    recursoSet(r, acao->estado ? "ON" : "OFF");
                }
                break;

                case AGEND_RESET:
                {
                    logaM(LOG_AVISO, "RESETANDO!");
                    vTaskDelay(pdTICKS_TO_MS(1000)); // Delay para dar tempo de flush nos logs
                    ESP.restart();
                }
                break;

                default:
                    logaM(LOG_CRITICO, "agendamentosProcessaTask :: TIPO [%d] invalido!", acao->tipo);
                    break;
                }

                // remove do slot
                acao->tipo = AGEND_NENHUM;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
