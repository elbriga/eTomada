#include <Arduino.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "display.h"
#include "mutex.h"

int regrasTotal = 3;
Regra regras[] = {
    regraCriaEvento(1, "B1", EVENTO_TOGGLE, "R10", ACAO_TOGGLE),
    regraCriaHorario(2, 8, 0, "R3", ACAO_ON),
    regraCriaHorario(3, 18, 30, "R3", ACAO_OFF),
};

void regrasInit()
{
}

void regraDisparaAcao(Regra *regra)
{
    Acao *acao = &regra->acao;

    logaMensagem(">> Regra[%d] acionada!", regra->id);

    // TODO loga regraGetTxt()

    switch (acao->tipo)
    {
    case ACAO_RECURSO:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        switch (acao->comando)
        {
        case ACAO_TOGGLE:
        {
            // TODO : e se nao for RELE??
            Rele *rele = recursoGetRele(rec);
            recursoSet(rec, !rele->estado);
        }
        break;

        default:
            logaMensagem("TODO :: regraDispara[%d] acao (%d)", regra->id, acao->comando);
            break;
        }
    }
    break;

    default:
        logaMensagem("TODO :: regraDispara[%d] tipo (%d)", regra->id, acao->tipo);
        break;
    }
}

void regrasProcessaEvento(Evento e)
{
    if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
    {
        return;
    }

    String msgDisplay = "";
    {
        MutexLock lockRecursos(recursosMutex);
        if (!lockRecursos)
        {
            logaMensagem("processaRegras: erro mutex");
            return;
        }

        // Iterar pelas regras
        for (int r = 0; r < regrasTotal; r++)
        {
            Regra *regra = &regras[r];

            // Verificar se foi o recurso da regra que gerou o evento
            if (strcmp(regra->condicao.recursoID, e.recurso->id))
                continue;

            bool disparaAcao = false;
            switch (regra->condicao.tipo)
            {
            case COND_EVENTO:
                if (e.tipo == regra->condicao.evento)
                {
                    disparaAcao = true;
                }
                break;

            default:
                break;
            }

            if (disparaAcao)
                // Executar!
                regraDisparaAcao(regra);
        }
    }

    if (msgDisplay != "")
    {
        displayMostraMsg(msgDisplay.c_str(), 5000, false);
    }
}

String regraValida(String regra)
{
    if (regra == "")
    {
        return "OK";
    }

    return "TODO";
}

Regra regraCriaEvento(
    uint16_t id,
    const char *recursoID,
    TipoEvento evento,
    const char *acaoRecurso,
    AcaoRecurso comando)
{
    Regra r{};

    r.id = id;
    r.ativa = true;

    r.condicao.tipo = COND_EVENTO;
    strlcpy(r.condicao.recursoID, recursoID, sizeof(r.condicao.recursoID));
    r.condicao.evento = evento;

    r.acao.tipo = ACAO_RECURSO;
    strlcpy(r.acao.recursoID, acaoRecurso, sizeof(r.acao.recursoID));
    r.acao.comando = comando;

    return r;
}

Regra regraCriaHorario(
    uint16_t id,
    uint8_t hora,
    uint8_t minuto,
    const char *recursoID,
    AcaoRecurso comando)
{
    Regra r{};

    r.id = id;
    r.ativa = true;

    r.condicao.tipo = COND_HORARIO;
    r.condicao.horario.hora = hora;
    r.condicao.horario.minuto = minuto;

    r.acao.tipo = ACAO_RECURSO;
    strlcpy(r.acao.recursoID, recursoID, sizeof(r.acao.recursoID));
    r.acao.comando = comando;

    return r;
}