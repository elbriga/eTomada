#include <Arduino.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "display.h"
#include "mutex.h"
#include "ntp.h"
#include "util.h"

#define MAX_REGRAS 64
#define REGRAS_PATH "/automacoes.json"
#define REGRAS_PATH_DEFAULT "/config/automacoesDefault.json"

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

String regrasLoad(const char *path);

void regrasInit()
{
    // Para Testes!
    // logaMensagem("INICIALIZANDO REGRAS FROM TESTES!!!");
    // utilCopiaArquivo("/config/automacoesTeste.json", REGRAS_PATH);

    if (!LittleFS.exists(REGRAS_PATH))
    {
        if (LittleFS.exists(REGRAS_PATH_DEFAULT))
        {
            logaMensagem("INICIALIZANDO REGRAS FROM DEFAULT");
            utilCopiaArquivo(REGRAS_PATH_DEFAULT, REGRAS_PATH);
        }
        else
        {
            logaMensagem("ERRO: regrasLoad > Arquivo [%s] nao existe!", REGRAS_PATH);
            return;
        }
    }

    String msg = regrasLoad(REGRAS_PATH);
    if (msg != "OK")
        logaMensagem(">> [%s]", msg.c_str());
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

        if (!regra->ativa)
            continue;

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

void regraLoadFromJSON(Regra *regra, JsonObject &doc)
{
    regra->id = doc["id"].as<int>() | (regrasTotal + 1); // TODO rever esse ID ao chamar fora de regrasLoad()
    regra->ativa = doc["ativa"].as<bool>();

    // preencher condicao
    String tipoCondicaoStr = doc["quando"]["tipo"].as<String>();
    if (tipoCondicaoStr == "EVENTO")
    {
        regra->condicao.tipo = COND_EVENTO;
        strlcpy(regra->condicao.recursoID,
                doc["quando"]["recurso"].as<const char *>(),
                sizeof(regra->condicao.recursoID));
        String eventoStr = doc["quando"]["evento"].as<String>();
        if (eventoStr == "TOGGLE")
            regra->condicao.evento = EVENTO_TOGGLE;
        else
        {
            logaMensagem("TipoEvento %s ??? Inativando regra", eventoStr.c_str());
            regra->ativa = false;
        }
    }
    else if (tipoCondicaoStr == "HORARIO")
    {
        regra->condicao.tipo = COND_HORARIO;
        regra->condicao.horario.hora = doc["quando"]["hora"].as<int>();
        regra->condicao.horario.minuto = doc["quando"]["minuto"].as<int>();
    }
    else
    {
        logaMensagem("TipoCondicao %s ??? Inativando regra", tipoCondicaoStr.c_str());
        regra->ativa = false;
    }

    // preencher acao
    String tipoAcaoStr = doc["acao"]["tipo"];
    if (tipoAcaoStr == "RECURSO")
    {
        regra->acao.tipo = ACAO_RECURSO;
        strlcpy(regra->acao.recursoID,
                doc["acao"]["recurso"].as<const char *>(),
                sizeof(regra->acao.recursoID));
        String acaoStr = doc["acao"]["comando"];
        if (acaoStr == "ON")
            regra->acao.comando = ACAO_ON;
        else if (acaoStr == "OFF")
            regra->acao.comando = ACAO_OFF;
        else if (acaoStr == "TOGGLE")
            regra->acao.comando = ACAO_TOGGLE;
        else if (acaoStr == "PULSE")
            regra->acao.comando = ACAO_PULSE;
        else
        {
            logaMensagem("AcaoRecurso %s ??? Inativando regra", acaoStr.c_str());
            regra->ativa = false;
        }
    }
    else
    {
        logaMensagem("TipoAcao %s ??? Inativando regra", tipoAcaoStr.c_str());
        regra->ativa = false;
    }
}

String regrasLoad(const char *path)
{
    File file = LittleFS.open(path, "r");
    if (!file)
        return "ERRO: regrasLoad > nao abriu";

    JsonDocument doc;
    DeserializationError erro = deserializeJson(doc, file);
    file.close();
    if (erro)
        return "ERRO: regrasLoad > lendo regras";

    JsonArray regrasJson = doc["regras"].as<JsonArray>();
    int totRegras = regrasJson.size();

    if (totRegras > MAX_REGRAS)
    {
        logaMensagem("MUITAS (%d) REGRAS NO ARQUIVO, LENDO SOMENTE %d PRIMEIRAS!!!", totRegras, MAX_REGRAS);
        totRegras = MAX_REGRAS;
    }

    if (regras)
        delete[] regras;

    regras = new Regra[totRegras]();
    if (!regras)
        utilDIE("NO new Regras! DIE!!!!!!!");

    regrasTotal = 0;
    for (JsonObject regraJson : regrasJson)
    {
        if (regrasTotal >= totRegras)
            break;

        Regra *regra = &regras[regrasTotal];

        regraLoadFromJSON(regra, regraJson);
        regraPrint(regra);

        regrasTotal++;
    }

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
    logaMensagem("Regra[%d][%s] > [%s]", r->id,
                 r->ativa ? "ON" : "OFF",
                 regraGetTxt(r).c_str());
}
