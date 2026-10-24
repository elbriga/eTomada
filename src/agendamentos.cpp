#include "agendamentos.h"
#include "recurso.h"
#include "loga.h"
#include "mutex.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("AGENDS", nivel, fmt, ##__VA_ARGS__)

struct AcaoAgendada
{
    TipoAgendamento tipo;
    uint32_t quando;
    uint16_t regraID;

    // Para acao AGEND_RECURSO
    char recursoID[32];
    char comando[16];
};

#define MAX_ACOES_AGENDADAS 16
static AcaoAgendada acoes[MAX_ACOES_AGENDADAS] = {};
static SemaphoreHandle_t agendamentosMutex = nullptr;

void agendamentosProcessaTask(void *);

void agendamentosInit()
{
    memset(acoes, 0, sizeof(acoes));

    agendamentosMutex = xSemaphoreCreateMutex();
    configASSERT(agendamentosMutex);

    xTaskCreatePinnedToCore(
        agendamentosProcessaTask,
        "agendamentos",
        4096,
        NULL,
        1,
        NULL,
        1);
}

void agendamentosLimpa(const char *recursoID)
{
    MutexLock lock(agendamentosMutex, "agendamentosLimpa");
    if (!lock)
        return;

    for (auto &acao : acoes)
        if (acao.tipo == AGEND_RECURSO && !strcmp(acao.recursoID, recursoID))
            acao = {};
}

// Sincronização > cancelar somente timers originados de regras.
void agendamentosLimpaRegras()
{
    MutexLock lock(agendamentosMutex, "agendamentosLimpaRegras");
    if (!lock)
        return;

    for (auto &acao : acoes)
        if (acao.tipo == AGEND_RECURSO && acao.regraID != 0)
            acao = {};
}

void agendamentosAdd(TipoAgendamento tipo, int timeoutMs, const char *recursoID, const char *comando, uint16_t regraID)
{
    if (timeoutMs > AGENDAMENTO_TEMPO_MAXIMO * 1000U)
    {
        logaM(LOG_CRITICO, "Timeout de agendamento excedido");
        return;
    }

    MutexLock lock(agendamentosMutex, "agendamentosAdd");
    if (!lock)
        return;

    for (auto &acao : acoes)
    {
        if (acao.tipo != AGEND_NENHUM)
            continue;

        AcaoAgendada nova = {};
        nova.tipo = tipo;
        nova.quando = millis() + timeoutMs;
        nova.regraID = regraID;

        if (recursoID)
            strlcpy(nova.recursoID, recursoID, sizeof(nova.recursoID));

        if (comando)
            strlcpy(nova.comando, comando, sizeof(nova.comando));

        acao = nova;
        return;
    }

    logaM(LOG_CRITICO, "agendamentosAdd: sem slots livres");
}

void agendamentosProcessaTask(void *)
{
    while (true)
    {
        // Serializa execução de timers com substituições de configuração.
        {
            MutexLock configLock(configMutex, "agendamentosTask config");
            if (configLock)
            {
                for (int s = 0; s < MAX_ACOES_AGENDADAS; s++)
                {
                    AcaoAgendada acao = {};
                    {
                        MutexLock lock(agendamentosMutex, "agendamentosTask");
                        if (!lock)
                            break;

                        if (acoes[s].tipo == AGEND_NENHUM ||
                            (int32_t)(millis() - acoes[s].quando) < 0)
                            continue;

                        // Retira o agendamento antes de executar.
                        acao = acoes[s];
                        acoes[s] = {};
                    }

                    // Execução sempre fora do mutex dos agendamentos.
                    switch (acao.tipo)
                    {
                    case AGEND_RECURSO:
                    {
                        logaM(LOG_AVISO,
                              "Agendamento [%s] no recurso [%s], regra [%u]",
                              acao.comando, acao.recursoID,
                              (unsigned)acao.regraID);

                        String msg;
                        if (!recursoSet(acao.recursoID, acao.comando, msg))
                            logaM(LOG_AVISO, "Agendamento: %s", msg.c_str());
                        break;
                    }

                    case AGEND_RESET:
                        logaM(LOG_AVISO, "RESETANDO!");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        ESP.restart();
                        break;

                    default:
                        logaM(LOG_CRITICO, "Agendamento invalido: %d", acao.tipo);
                        break;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
