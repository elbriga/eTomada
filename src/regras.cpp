#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "display.h"
#include "mutex.h"
#include "ntp.h"
#include "util.h"
#include "recurso.h"
#include "agendamentos.h"
#include "sensorChuva.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("REGRA", nivel, fmt, ##__VA_ARGS__)

#define MAX_REGRAS 64

int regrasTotal = 0;
Regra *regras = nullptr;

void regrasBoot();
String regrasLoad(const char *path);
String regraGetTxt(Regra *r);
void regraLoadFromJSON(Regra *regra, JsonObject &doc);
String regrasPersiste(Regra *novaRegra);
String regraGetCondicaoTxt(Condicao *c);

void regrasInit()
{
    if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
        return;

    if (!LittleFS.exists(REGRAS_PATH))
    {
        if (LittleFS.exists(REGRAS_PATH_DEFAULT))
        {
            logaM(LOG_AVISO, "INICIALIZANDO REGRAS FROM DEFAULT");
            utilCopiaArquivo(REGRAS_PATH_DEFAULT, REGRAS_PATH);
        }
        else
        {
            logaM(LOG_AVISO, "ERRO: regrasLoad > Arquivo [%s] nao existe!", REGRAS_PATH);
            return;
        }
    }

    String msg = regrasLoad(REGRAS_PATH);
    if (msg != "OK")
        logaM(LOG_AVISO, ">> regrasLoad: [%s]", msg.c_str());

    regrasBoot();
}

Regra *regrasCalculaEstadoAtual(Recurso *recursoIn, bool *estadoAtualOut)
{
    // Tratando somente reles por enquanto
    if (recursoIn->tipo != RECURSO_RELE)
        return nullptr;

    struct tm timeinfo;
    sysGetTime(&timeinfo);
    int minutoAtual = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int minutoUltimo = -1;

    Regra *regraAtivadaOut = nullptr;
    for (int r = 0; r < regrasTotal; r++)
    {
        Regra *regra = &regras[r];

        // Trabalhar com regras de HORARIO
        Condicao *cond = &regra->condicao[0]; // TODO fixo na condição 1
        if (cond->tipo != COND_HORARIO)
            continue;

        // Trabalhar em cima de ON e OFF
        if (regra->acao.comando != COMANDO_ON && regra->acao.comando != COMANDO_OFF)
            continue;

        // Verificar se esta regra age em cima do recurso
        if (strcmp(regra->acao.recursoID, recursoIn->id))
            continue;

        // Verificar se ja passou esse HORARIO
        int minutoRegra = cond->horario.hora * 60 + cond->horario.minuto;
        if (minutoRegra < minutoAtual)
        {
            // Salvar o estado da ultima regra aplicavel
            if (minutoRegra > minutoUltimo)
            {
                minutoUltimo = minutoRegra;
                regraAtivadaOut = regra;
                *estadoAtualOut = (regra->acao.comando == COMANDO_ON);
            }
        }
    }

    return regraAtivadaOut;
}

void regrasBoot()
{
    // Obter horario
    struct tm timeinfo;
    sysGetTime(&timeinfo);
    if (timeinfo.tm_year + 1900 < 2026)
    {
        // Sem data/hora não processa regras de HORARIO
        logaM(LOG_AVISO, "Pulando Boot das regras!!! estamos sem HORA!!");
        return;
    }

    logaM(LOG_AVISO, "== regrasBoot() ==");

    // Ajustar o estado dos RELEs conforme as regras de HORARIO para agora
    int totRecursos = recursosGetCount();
    for (int r = 0; r < totRecursos; r++)
    {
        Recurso *recurso = recursoGetPorIndice(r);
        if (recurso->tipo != RECURSO_RELE)
            continue;

        bool estadoAtual;
        Regra *regraAtivada = regrasCalculaEstadoAtual(recurso, &estadoAtual);
        if (regraAtivada)
        {
            logaM(LOG_NORMAL, "Conferir estado do recurso [%s][%s] para %d pela regra [%s]",
                  recurso->id, recurso->nome, estadoAtual, regraAtivada->nome);
            String msg = recursoCheck(recurso, estadoAtual);
            if (msg != "")
                logaM(LOG_AVISO, ">> recursoCheck :: [%s]", msg.c_str());
        }
    }
}

Regra *regraGet(int id)
{
    int tot = regrasCount();
    for (int r = 0; r < tot; r++)
    {
        if (regras[r].id == id)
            return &regras[r];
    }
    return NULL;
}

Regra *regraGetPorIndice(int i)
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

    logaM(LOG_NORMAL, ">> Ativando [%s]", regraGetTxt(regra).c_str());

    switch (acao->tipo)
    {
    case ACAO_ESTADO:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        if (!rec || rec->tipo != RECURSO_RELE)
            return "dispAcaoESTADO : Nao eh RELE!";

        return recursoSet(rec, acao->comando);
    }
    break;

    case ACAO_TIMER:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        if (!rec || rec->tipo != RECURSO_RELE)
            return "dispAcaoTIMER : Nao eh RELE!";

        String ret = recursoSet(rec, COMANDO_ON);
        // Agendar o OFF
        // TODO :: no recursoSet cancelar os agendamentos
        agendamentosAdd(AGEND_RECURSO, acao->timer * 1000, rec->id, false);
    }
    break;

    default:
        logaM(LOG_CRITICO, "TODO :: regraDispara[%d] tipo (%d)", regra->id, acao->tipo);
        break;
    }

    return "ToDo!";
}

int regrasGetValorPorNome(const char *nomeVar)
{
    if (!strcmp(nomeVar, "CHUVA"))
        return sensorChuvaGetHorasSemChuva();

    Recurso *r = recursoGet(nomeVar);
    if (r)
        return recursoGetValor(r);

    return -1;
}

void regrasProcessaEvento(Evento e)
{
    if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
    {
        return;
    }

    String msgDisplay = "";
    for (int r = 0; r < regrasTotal; r++)
    {
        Regra *regra = &regras[r];

        if (!regra->ativa)
            continue;

        bool disparaAcao = false;

        // Verificas as condicoes
        for (int i = 0; i < REGRAS_MAX_CONDICOES; i++)
        {
            Condicao *c = &regra->condicao[i];
            if (c->tipo == COND_NENHUMA) // FIM
                break;

            switch (c->tipo)
            {
            case COND_EVENTO:
                // Verificar se foi o recurso da regra que gerou o evento
                if (e.recurso && strcmp(c->evento.recursoID, e.recurso->id))
                    continue;

                if (e.tipo == c->evento.tipo)
                    disparaAcao = true;
                break;

            case COND_HORARIO:
                if (e.tipo != EVENTO_HORARIO)
                    break;

                // Obter horario
                struct tm timeinfo;
                sysGetTime(&timeinfo);

                if (timeinfo.tm_hour == c->horario.hora && timeinfo.tm_min == c->horario.minuto)
                {
                    if (timeinfo.tm_year + 1900 < 2026)
                    {
                        // Sem data/hora não processa regras de HORARIO
                        logaM(LOG_AVISO, "Pulando regra[%s] : estamos sem HORA!", regra->nome);
                        break;
                    }
                    disparaAcao = true;
                }
                break;

            case COND_EXPRESSAO:
            {
                int valorVar = regrasGetValorPorNome(c->expressao.variavel);
                if (c->expressao.op[0] == '>')
                    disparaAcao = (c->expressao.op[1] == '=')
                                      ? valorVar >= c->expressao.valor
                                      : valorVar > c->expressao.valor;
                else if (c->expressao.op[0] == '<')
                    disparaAcao = (c->expressao.op[1] == '=')
                                      ? valorVar <= c->expressao.valor
                                      : valorVar < c->expressao.valor;
                else if (c->expressao.op[0] == '!')
                    disparaAcao = valorVar != c->expressao.valor;
                else if (c->expressao.op[0] == '=')
                    disparaAcao = valorVar == c->expressao.valor;
                else
                {
                    logaM(LOG_CRITICO, "regrasProcessaEvento[%d] condicao.op (%s) DESCONHECIDA", regra->id, c->expressao.op);
                    disparaAcao = false;
                }
            }
            break;

            default:
                logaM(LOG_CRITICO, "TODO :: regrasProcessaEvento[%d] condicao.tipo (%d) DESCONHECIDA", regra->id, c->tipo);
                disparaAcao = false;
                break;
            }

            // As condicionais são concatenadas com "E", se uma falhar já era!
            if (!disparaAcao)
            {
                // Dar mensagem se falhar depois da primeira condicional
                if (i)
                    logaM(LOG_NORMAL, "Regra[%s]: nao disparou pela condicional [%s]",
                          regra->nome, regraGetCondicaoTxt(c).c_str());
                break;
            }
        }

        if (disparaAcao)
        {
            // Executar!
            msgDisplay = regraDisparaAcao(regra);
            logaM(LOG_NORMAL, "Resultado da Regra[%s]: [%s]", regra->nome, msgDisplay.c_str());
        }
    }

    if (msgDisplay != "")
        displayMostraMsg(msgDisplay.c_str(), 5000, false);
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
    // case EVENTO_PRESSIONOU:
    //     return "PRESSIONOU";
    // case EVENTO_SOLTOU:
    //     return "SOLTOU";
    case EVENTO_CLICK:
        return "CLICK";
    case EVENTO_DOUBLE_CLICK:
        return "DUPCLICK";
    // case EVENTO_LONG_PRESS:
    //     return "LONG_PRESS";
    case EVENTO_VALOR_MUDOU:
        return "CHANGED";
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
    case ACAO_ESTADO:
        return "ESTADO";
    case ACAO_TIMER:
        return "TIMER";
    default:
        return "ACAO??";
    }
}

String regraGetCondicaoTxt(Condicao *c)
{
    String ret;
    ret.reserve(64);

    switch (c->tipo)
    {
    case COND_EVENTO:
        ret += "QUANDO ";
        ret += c->evento.recursoID;
        ret += ":";
        ret += regraTipoEventoTxt(c->evento.tipo);
        break;

    case COND_HORARIO:
        ret += "AS ";
        if (c->horario.hora < 10)
            ret += "0";
        ret += c->horario.hora;
        ret += ":";
        if (c->horario.minuto < 10)
            ret += "0";
        ret += c->horario.minuto;
        break;

    case COND_EXPRESSAO:
        ret += "SE ";
        ret += c->expressao.variavel;
        ret += " ";
        ret += c->expressao.op;
        ret += " ";
        ret += c->expressao.valor;
        break;

    default:
        ret += "CONDICAO?";
        break;
    }

    return ret;
}

String regraGetTxt(Regra *r)
{
    String ret;

    ret.reserve(96);

    // Condição
    for (int i = 0; i < REGRAS_MAX_CONDICOES; i++)
    {
        Condicao *c = &r->condicao[i];
        if (c->tipo == COND_NENHUMA)
            break;

        if (i)
            ret += ",";

        ret += regraGetCondicaoTxt(c);
    }

    ret += " -> ";

    // Ação
    switch (r->acao.tipo)
    {
    case ACAO_ESTADO:
        ret += r->acao.recursoID;
        ret += ":";
        ret += comandoRecursoGetString(r->acao.comando);
        break;

    case ACAO_TIMER:
        ret += r->acao.recursoID;
        ret += " TIMER:";
        ret += r->acao.timer;
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

JsonDocument regraGetCondicoesJSONDoc(Regra *r)
{
    JsonArray doc;

    for (int i = 0; i < REGRAS_MAX_CONDICOES; i++)
    {
        Condicao *c = &r->condicao[i];
        if (c->tipo == COND_NENHUMA)
            break;

        JsonObject condicao;
        condicao["tipo"] = regraTipoCondicaoTxt(c->tipo);
        switch (c->tipo)
        {
        case COND_EVENTO:
            condicao["recurso"] = c->evento.recursoID;
            condicao["evento"] = regraTipoEventoTxt(c->evento.tipo);
            break;

        case COND_HORARIO:
            condicao["hora"] = c->horario.hora;
            condicao["minuto"] = c->horario.minuto;
            break;

        case COND_EXPRESSAO:
            condicao["variavel"] = c->expressao.variavel;
            condicao["operacao"] = c->expressao.op;
            condicao["valor"] = c->expressao.valor;
            break;

        default:
            logaM(LOG_AVISO, "regraGetCondicoesJSONDoc[%s] :: tipoCondicao[%d] invalido", r->nome, c->tipo);
        }

        doc.add(condicao);
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
    case ACAO_ESTADO:
        doc["recurso"] = a->recursoID;
        doc["comando"] = comandoRecursoGetString(a->comando);
        break;

    case ACAO_TIMER:
        doc["recurso"] = a->recursoID;
        doc["timer"] = a->timer;
        break;
    }

    return doc;
}

void regraGetJS(Regra *r, JsonObject &doc)
{
    doc["id"] = r->id;
    doc["nome"] = r->nome;
    doc["ativa"] = r->ativa;

    doc["descricao"] = regraGetTxt(r);

    doc["quando"] = regraGetCondicoesJSONDoc(r);
    doc["acao"] = regraGetAcaoJSONDoc(r);
}

void regrasGetJSONDoc(JsonDocument &doc, Regra *novaRegra)
{
    JsonArray regrasOut = doc.to<JsonArray>();

    int totRegras = regrasCount();
    for (int r = 0; r < totRegras; r++)
    {
        Regra *regra = regraGetPorIndice(r);
        JsonObject obj = regrasOut.add<JsonObject>();
        regraGetJS(regra, obj);
    }

    if (novaRegra)
    {
        // Add!
        JsonObject obj = regrasOut.add<JsonObject>();
        regraGetJS(novaRegra, obj);
    }
}

String regraAtualizaFromJSON(uint8_t *json)
{
    JsonDocument doc;
    if (utilLeJson("regraAtualizaFromJSON", doc, json))
        return "JSON Invalido";

    int id = doc["id"].as<int>();
    bool addRegra = (id == 0);

    Regra *regra, novaRegra;

    if (addRegra)
    {
        memset(&novaRegra, 0, sizeof(novaRegra));
        regra = &novaRegra;
    }
    else
    {
        regra = regraGet(id);
        if (!regra)
        {
            doc.clear();
            return "Regra Invalida";
        }
    }

    JsonObject obj = doc.as<JsonObject>();
    regraLoadFromJSON(regra, obj);
    doc.clear();

    // Se passar a novaRegra para o regrasPersiste() ele adiciona ela no final
    String msg = regrasPersiste(addRegra ? &novaRegra : nullptr);
    if (msg != "OK")
        return msg;

    if (addRegra)
    {
        // Recarregar as regras para a nova regra entrar no array global de regras
        msg = regrasLoad(REGRAS_PATH);
        if (msg != "OK")
            return msg;
    }

    return "OK";
}

String regrasPersiste(Regra *novaRegra)
{
    File file = LittleFS.open("/automacoes.json.tmp", "w");
    if (!file)
    {
        return "ERRO: ao abrir automacoes.json.tmp para escrita";
    }

    JsonDocument regras;
    regrasGetJSONDoc(regras, novaRegra); // Se novaRegra != nullptr == ADD

    JsonDocument doc;
    doc["regras"] = regras;

    String out;
    if (!serializeJson(doc, out))
    {
        file.close();
        return "ERRO: regrasPersiste:serializeJson";
    }

    logaM(LOG_DEBUG, "regrasPersiste: [%s]", out.c_str());

    if (!serializeJson(doc, file))
    {
        file.close();
        return "ERRO: regrasPersiste:serializeJson FILE";
    }

    file.close();

    LittleFS.rename("/automacoes.json.tmp", "/automacoes.json");

    logaM(LOG_NORMAL, "Regras Salvas!");

    return "OK";
}

int regraFindNextID()
{
    int MAX = 0, totRegras = regrasCount();
    for (int r = 0; r < totRegras; r++)
    {
        Regra *regra = regraGetPorIndice(r);
        if (regra->id > MAX)
            MAX = regra->id;
    }
    return MAX + 1;
}

void regraLoadFromJSON(Regra *regra, JsonObject &doc)
{
    regra->id = doc["id"].as<int>();
    if (!regra->id)
        regra->id = regraFindNextID();

    if (doc["nome"])
        strlcpy(regra->nome, doc["nome"].as<const char *>(), sizeof(regra->nome));

    if (doc["ativa"])
        regra->ativa = doc["ativa"].as<bool>();

    // preencher condicao
    JsonArray condicoes = doc["quando"];
    if (!condicoes.size())
    {
        logaM(LOG_CRITICO, "Sem condicoes! Inativando regra[%d]", regra->id);
        regra->ativa = false;
    }
    else
    {
        int idxCondicao = 0;
        for (JsonObject condicao : condicoes)
        {
            if (idxCondicao >= REGRAS_MAX_CONDICOES)
            {
                logaM(LOG_AVISO, "Regra[%d] com muitas condicoes! Cortando!", regra->id);
                break;
            }

            Condicao *condPtr = &regra->condicao[idxCondicao++];

            String tipoCondicaoStr = condicao["tipo"].as<String>();
            if (tipoCondicaoStr == "EVENTO")
            {
                condPtr->tipo = COND_EVENTO;
                strlcpy(condPtr->evento.recursoID, condicao["recurso"].as<const char *>(), sizeof(condPtr->evento.recursoID));

                String eventoStr = condicao["evento"].as<String>();
                if (eventoStr == "TOGGLE")
                    condPtr->evento.tipo = EVENTO_TOGGLE;
                else if (eventoStr == "CLICK")
                    condPtr->evento.tipo = EVENTO_CLICK;
                else if (eventoStr == "LIGOU")
                    condPtr->evento.tipo = EVENTO_LIGOU;
                else if (eventoStr == "DESLIGOU")
                    condPtr->evento.tipo = EVENTO_DESLIGOU;
                else if (eventoStr == "DUPCLICK")
                    condPtr->evento.tipo = EVENTO_DOUBLE_CLICK;
                else
                // TODO :: outros eventos
                {
                    logaM(LOG_CRITICO, "TipoEvento %s ??? Inativando regra[%d]", eventoStr.c_str(), regra->id);
                    regra->ativa = false;
                }
            }
            else if (tipoCondicaoStr == "HORARIO")
            {
                condPtr->tipo = COND_HORARIO;
                condPtr->horario.hora = condicao["hora"].as<int>();
                condPtr->horario.minuto = condicao["minuto"].as<int>();
            }
            else if (tipoCondicaoStr == "EXPRESSAO")
            {
                condPtr->tipo = COND_EXPRESSAO;
                strlcpy(condPtr->expressao.variavel, condicao["variavel"].as<const char *>(), sizeof(condPtr->expressao.variavel));
                strlcpy(condPtr->expressao.op, condicao["operacao"].as<const char *>(), sizeof(condPtr->expressao.op));
                condPtr->expressao.valor = condicao["valor"].as<int>();
            }
            else
            {
                logaM(LOG_CRITICO, "TipoCondicao %s ??? Inativando regra[%d]", tipoCondicaoStr.c_str(), regra->id);
                regra->ativa = false;
            }
        }
    }

    // preencher acao
    if (doc["acao"]["tipo"])
    {
        String tipoAcaoStr = doc["acao"]["tipo"];
        if (tipoAcaoStr == "ESTADO")
        {
            regra->acao.tipo = ACAO_ESTADO;
            strlcpy(regra->acao.recursoID,
                    doc["acao"]["recurso"].as<const char *>(),
                    sizeof(regra->acao.recursoID));
            String acaoStr = doc["acao"]["comando"];
            regra->acao.comando = comandoRecursoGetFromString(acaoStr);
        }
        else if (tipoAcaoStr == "TIMER")
        {
            regra->acao.tipo = ACAO_TIMER;
            strlcpy(regra->acao.recursoID,
                    doc["acao"]["recurso"].as<const char *>(),
                    sizeof(regra->acao.recursoID));
            regra->acao.timer = doc["acao"]["timer"].as<uint32_t>();
        }
        else
        {
            logaM(LOG_CRITICO, "TipoAcao %s ??? Inativando regra[%d]", tipoAcaoStr.c_str(), regra->id);
            regra->ativa = false;
        }
    }
}

String regrasLoad(const char *path)
{
    File file = LittleFS.open(path, "r");
    if (!file)
        return "ERRO: regrasLoad > nao abriu";

    JsonDocument doc;
    DeserializationError erro = utilLeJson("regrasLoad", doc, file);
    file.close();
    if (erro)
        return "ERRO: regrasLoad > lendo regras";

    JsonArray regrasJson = doc["regras"].as<JsonArray>();
    int totRegras = regrasJson.size();

    if (totRegras > MAX_REGRAS)
    {
        logaM(LOG_CRITICO, "MUITAS (%d) REGRAS NO ARQUIVO, LENDO SOMENTE %d PRIMEIRAS!!!", totRegras, MAX_REGRAS);
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

    doc.clear();

    return "OK";
}

void regraPrint(Regra *r)
{
    logaM(LOG_NORMAL, "Regra[%d][%s][%s] > [%s]",
          r->id, r->ativa ? "ON" : "OFF", r->nome,
          regraGetTxt(r).c_str());
}
