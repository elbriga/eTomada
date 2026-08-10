#include <Arduino.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "display.h"
#include "mutex.h"
#include "ntp.h"

/*
int regrasTotal = 4;
Regra regras[] = {
    regraCriaEvento(1, "B1", EVENTO_TOGGLE, "R10", ACAO_TOGGLE),
    regraCriaEvento(2, "B2", EVENTO_TOGGLE, "R9", ACAO_PULSE),
    regraCriaHorario(3, 18, 45, "R2", ACAO_ON),
    regraCriaHorario(4, 18, 55, "R2", ACAO_OFF),
};
*/

int regrasTotal = 0;
Regra *regras = nullptr;

String regrasLoad();

void regrasInit()
{
    regrasLoad();
}

Regra *regraGet(int i)
{
    if (i >= 0 && i < regrasCount())
        return &regras[i];

    return NULL;
}

int regrasCount()
{
    return regrasTotal;
}

String regraGetTxt(Regra *r);
String regraDisparaAcao(Regra *regra)
{
    Acao *acao = &regra->acao;

    logaMensagem(">> [%s]", regraGetTxt(regra).c_str());

    switch (acao->tipo)
    {
    case ACAO_RECURSO:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        if (rec->tipo != RECURSO_RELE)
            return "dispAcaoRECURSO : Nao eh RELE!";

        switch (acao->comando)
        {
        case ACAO_ON:
            return recursoSet(rec, "ON");
        case ACAO_OFF:
            return recursoSet(rec, "OFF");
        case ACAO_TOGGLE:
            return recursoSet(rec, "TOGGLE");
        case ACAO_PULSE:
            return recursoSet(rec, "PULSE");
        }
    }
    break;

    default:
        logaMensagem("TODO :: regraDispara[%d] tipo (%d)", regra->id, acao->tipo);
        break;
    }

    return "ToDo!";
}

void regrasProcessaEvento(Evento e)
{
    if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
    {
        return;
    }

    String msgDisplay = "";
    // Iterar pelas regras
    for (int r = 0; r < regrasTotal; r++)
    {
        Regra *regra = &regras[r];

        // Verificar se foi o recurso da regra que gerou o evento
        if (e.recurso && strcmp(regra->condicao.recursoID, e.recurso->id))
            continue;

        bool disparaAcao = false;
        switch (regra->condicao.tipo)
        {
        case COND_EVENTO:
            if (e.tipo == regra->condicao.evento)
                disparaAcao = true;
            break;

        case COND_HORARIO:
            if (e.tipo == EVENTO_HORARIO)
            {
                // Obter horario
                struct tm timeinfo;
                ntpGetTime(&timeinfo);

                if (timeinfo.tm_hour == regra->condicao.horario.hora && timeinfo.tm_min == regra->condicao.horario.minuto)
                {
                    if (timeinfo.tm_year < 2026)
                    {
                        // Sem data/hora não processa regras de HORARIO
                        logaMensagem("Pulando regra[%d] : estamos sem HORA!", r);
                        break;
                    }
                    disparaAcao = true;
                }
            }
            break;

        default:
            logaMensagem("TODO :: regrasProcessaEvento[%d] condicao.tipo (%d)", regra->id, regra->condicao.tipo);
            break;
        }

        if (disparaAcao)
            // Executar!
            msgDisplay = regraDisparaAcao(regra);
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

static const char *regraTipoEventoTxt(TipoEvento evento)
{
    switch (evento)
    {
    case EVENTO_NENHUM:
        return "NENHUM";
    case EVENTO_LIGOU:
        return "LIGOU";
    case EVENTO_DESLIGOU:
        return "DESLIGOU";
    case EVENTO_TOGGLE:
        return "TOGGLE";
    case EVENTO_PRESSIONOU:
        return "PRESSIONOU";
    case EVENTO_SOLTOU:
        return "SOLTOU";
    case EVENTO_CLICK:
        return "CLICK";
    case EVENTO_DOUBLE_CLICK:
        return "DOUBLE_CLICK";
    case EVENTO_LONG_PRESS:
        return "LONG_PRESS";
    case EVENTO_VALOR_MUDOU:
        return "VALOR_MUDOU";
    case EVENTO_HORARIO:
        return "HORARIO";
    default:
        return "?";
    }
}

static const char *regraTipoCondicaoTxt(TipoCondicao condicao)
{
    switch (condicao)
    {
    case COND_EVENTO:
        return "EVENTO";
    case COND_HORARIO:
        return "HORARIO";
    default:
        return "COND??";
    }
}

static const char *regraTipoAcaoTxt(TipoAcao acao)
{
    switch (acao)
    {
    case ACAO_RECURSO:
        return "RECURSO";
    default:
        return "ACAO??";
    }
}

static const char *regraAcaoRecursoTxt(AcaoRecurso comando)
{
    switch (comando)
    {
    case ACAO_ON:
        return "ON";
    case ACAO_OFF:
        return "OFF";
    case ACAO_TOGGLE:
        return "TOGGLE";
    case ACAO_PULSE:
        return "PULSE";
    default:
        return "?";
    }
}

String regraGetTxt(Regra *r)
{
    String ret;

    ret.reserve(96);

    ret += "Regra[";
    ret += r->id;
    ret += "] ";

    if (!r->ativa)
        ret += "(INATIVA) ";

    // Condição
    switch (r->condicao.tipo)
    {
    case COND_EVENTO:
        ret += "SE ";
        ret += r->condicao.recursoID;
        ret += ":";
        ret += regraTipoEventoTxt(r->condicao.evento);
        break;

    case COND_HORARIO:
        ret += "AS ";
        if (r->condicao.horario.hora < 10)
            ret += "0";
        ret += r->condicao.horario.hora;
        ret += ":";
        if (r->condicao.horario.minuto < 10)
            ret += "0";
        ret += r->condicao.horario.minuto;
        break;

    case COND_ESTADO:
        ret += "ESTADO ";
        ret += r->condicao.recursoID;
        ret += " ";
        ret += r->condicao.estado.valor;
        break;

    case COND_EXPRESSAO:
        ret += "EXPRESSAO";
        break;

    default:
        ret += "CONDICAO?";
        break;
    }

    ret += " -> ";

    // Ação
    switch (r->acao.tipo)
    {
    case ACAO_RECURSO:
        ret += r->acao.recursoID;
        ret += ":";
        ret += regraAcaoRecursoTxt(r->acao.comando);
        break;

    case ACAO_DELAY:
        ret += "DELAY:";
        ret += r->acao.delay;
        break;

    case ACAO_SCRIPT:
        ret += "SCRIPT";
        break;

    default:
        ret += "ACAO?";
        break;
    }

    return ret;
}

JsonDocument regraGetCondicaoJSONDoc(Regra *r)
{
    JsonDocument doc;
    Condicao *c = &r->condicao;

    doc["tipo"] = regraTipoCondicaoTxt(c->tipo);

    switch (c->tipo)
    {
    case COND_EVENTO:
        doc["recurso"] = c->recursoID;
        doc["evento"] = regraTipoEventoTxt(c->evento);
        break;

    case COND_HORARIO:
        doc["hora"] = c->horario.hora;
        doc["minuto"] = c->horario.minuto;
        break;
    }

    return doc;
}

JsonDocument regraGetAcaoJSONDoc(Regra *r)
{
    JsonDocument doc;
    Acao *a = &r->acao;

    doc["tipo"] = regraTipoAcaoTxt(a->tipo);

    switch (a->tipo)
    {
    case ACAO_RECURSO:
        doc["comando"] = regraAcaoRecursoTxt(a->comando);
        doc["recurso"] = a->recursoID;
        break;
    }

    return doc;
}

JsonDocument regraGetJSONDoc(Regra *r)
{
    JsonDocument doc;

    doc["id"] = r->id;
    doc["ativa"] = r->ativa;

    // doc["descricao"] = regraGetTxt(r);

    doc["quando"] = regraGetCondicaoJSONDoc(r);
    doc["acao"] = regraGetAcaoJSONDoc(r);

    return doc;
}

String regrasPersiste()
{
    File file = LittleFS.open("/automacoes.json.tmp", "w");
    if (!file)
    {
        return "ERRO: ao abrir automacoes.json.tmp para escrita";
    }

    JsonDocument doc;
    JsonArray regrasOut = doc["regras"].to<JsonArray>();

    int totRegras = regrasCount();
    for (int r = 0; r < totRegras; r++)
    {
        Regra *regra = regraGet(r);
        regrasOut.add(regraGetJSONDoc(regra));
    }

    String out;
    if (!serializeJson(doc, out))
    {
        file.close();
        return "ERRO: regrasPersiste:serializeJson";
    }

    logaMensagem("regrasPersiste: [%s]", out.c_str());

    if (!serializeJson(doc, file))
    {
        file.close();
        return "ERRO: regrasPersiste:serializeJson FILE";
    }

    file.close();

    LittleFS.rename("/automacoes.json.tmp", "/automacoes.json");

    logaMensagem("Regras Salvas!");

    return out;
}

String regrasLoad()
{
    regrasTotal = 0;

    if (!LittleFS.exists("/automacoes.json"))
    {
        if (LittleFS.exists("/config/automacoes.json"))
        {
            // TODO !! Copiar o arquivo de exemplo
            // utilCopiaArquivo("/config/automacoes.json", "/automacoes.json");
        }
        else
        {
            return "ERRO: regrasLoad > Arquivo /automacoes.json nao existe!";
        }
    }

    File file = LittleFS.open("/automacoes.json", "r");
    if (!file)
        return "ERRO: regrasLoad > nao abriu /automacoes.json";

    JsonDocument doc;
    DeserializationError erro = deserializeJson(doc, file);
    file.close();
    if (erro)
        return "ERRO: regrasLoad > lendo regras";

    JsonArray regrasJson = doc["regras"].as<JsonArray>();
    regrasJson.size();
    /*
        for (JsonObject r : regrasJson)
        {
            if (regrasTotal >= MAX_REGRAS)
                break;

            Regra *regra = &regras[totalRegras];

            regra->id = r["id"] | 0;

            // preencher condicao
            // preencher acao

            totalRegras++;
        }
    */
    logaMensagem("Regras carregadas: %d", regrasTotal);
    return "OK";
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

void regraPrint(Regra *r)
{
    logaMensagem("Regra[%d] > [%s]", r->id, regraGetTxt(r).c_str());
}
