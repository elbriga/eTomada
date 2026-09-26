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
#include "http.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("REGRA", nivel, fmt, ##__VA_ARGS__)

#define MAX_REGRAS 64

int regrasTotal = 0;
Regra *regras = nullptr;

void regrasBoot();
String regrasLoad(const char *path);
String regraGetTxt(Regra *r);
void regraLoadFromJSON(Regra *regra, JsonObject &doc);
String regrasPersiste(Regra *novaRegra = nullptr);

void regrasInit()
{
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
}

Regra *regrasCalculaEstadoAtual(Recurso *recursoIn, String &estadoAtualOut)
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
        Condicao *cond = &regra->condicao;
        if (cond->tipo != COND_HORARIO)
            continue;

        // Trabalhar em cima de ON e OFF
        if (regra->acao.comando != "ON" && regra->acao.comando != "OFF")
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
                estadoAtualOut = regra->acao.comando;
            }
        }
    }

    return regraAtivadaOut;
}

#define REGRAS_BOOT_MAX_ACOES 16
typedef struct
{
    String recursoID, estado;
} CacheAcao;

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

    int totAcoes = 0;
    CacheAcao cache[REGRAS_BOOT_MAX_ACOES] = {};

    {
        MutexLock lock(recursosMutex);
        if (!lock)
        {
            logaM(LOG_CRITICO, "regrasBoot: mutex timeout");
            return;
        }

        logaM(LOG_AVISO, "== regrasBoot() ==");

        // Ajustar o estado dos RELEs conforme as regras de HORARIO para agora
        int totRecursos = recursosGetCount();
        for (int r = 0; r < totRecursos; r++)
        {
            Recurso *recurso = recursoGetPorIndice(r);
            if (recurso->tipo != RECURSO_RELE) // TODO Umid?
                continue;

            String estadoAtual;
            Regra *regraAtivada = regrasCalculaEstadoAtual(recurso, estadoAtual);
            if (regraAtivada)
            {
                logaM(LOG_NORMAL, "Setar recurso [%s][%s] para %d pela regra [%s]",
                      recurso->id, recurso->nome, estadoAtual.c_str(), regraAtivada->nome);

                // Verificar se ja temos esse recurso no cache
                // Se houver o recurso ficara no estado da ultima regra ativada
                bool temos = false;
                for (int c = 0; c < totAcoes; c++)
                {
                    if (cache[c].recursoID == recurso->id)
                    {
                        cache[c].estado = estadoAtual;
                        temos = true;
                        break;
                    }
                }
                if (temos)
                    continue;

                if (totAcoes >= REGRAS_BOOT_MAX_ACOES)
                {
                    logaM(LOG_CRITICO, "REGRAS_BOOT_MAX_ACOES atingido!!!");
                    break;
                }

                cache[totAcoes].recursoID = recurso->id;
                cache[totAcoes].estado = estadoAtual;
                totAcoes++;
            }
        }
    }

    // Executar o cache fora do Lock
    for (int c = 0; c < totAcoes; c++)
    {
        recursoSet(cache[c].recursoID.c_str(), cache[c].estado.c_str());
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

Regra *regraGetPorRecurso(const char *idLocal)
{
    int tot = regrasCount();
    for (int r = 0; r < tot; r++)
    {
        Regra *regra = &regras[r];
        if (regra->condicao.tipo == COND_EVENTO && !strcmp(regra->condicao.evento.recursoID, idLocal))
            return regra;
        if (regra->acao.tipo == ACAO_ESTADO && !strcmp(regra->acao.recursoID, idLocal))
            return regra;
        if (regra->acao.tipo == ACAO_TIMER && !strcmp(regra->acao.recursoID, idLocal))
            return regra;
        if (!strcmp(regra->condicao.check.variavel, idLocal))
            return regra;
    }
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

        return recursoSet(acao->recursoID, acao->comando);
    }
    break;

    case ACAO_TIMER:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        if (!rec || rec->tipo != RECURSO_RELE)
            return "dispAcaoTIMER : Nao eh RELE!";

        String ret = recursoSet(acao->recursoID, "ON");
        // Agendar o OFF
        // TODO :: no recursoSet cancelar os agendamentos
        agendamentosAdd(AGEND_RECURSO, acao->timer * 1000, acao->recursoID, false);
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
    if (!strcmp(nomeVar, SENSORCHUVA_RECURSOID))
        return sensorChuvaGetHorasSemChuva();

    Recurso *r = recursoGet(nomeVar);
    if (r)
        return recursoGetValor(r);

    return -1;
}

void regrasProcessaEvento(Evento e)
{
    String msgDisplay = "";
    for (int r = 0; r < regrasTotal; r++)
    {
        Regra *regra = &regras[r];

        if (!regra->ativa)
            continue;

        bool disparaAcao = false;

        // Verificas a condicao
        Condicao *c = &regra->condicao;
        if (c->tipo == COND_NENHUMA) // ??
            continue;

        switch (c->tipo)
        {
        case COND_EVENTO:
            // Verificar se foi o recurso da regra que gerou o evento
            if (strcmp(c->evento.recursoID, e.recursoID))
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

        default:
            logaM(LOG_CRITICO, "TODO :: regrasProcessaEvento[%d] condicao.tipo (%d) DESCONHECIDA", regra->id, c->tipo);
            disparaAcao = false;
            break;
        }

        // Verificar o check
        if (disparaAcao && c->check.variavel[0] != '\0')
        {
            int valorVar = regrasGetValorPorNome(c->check.variavel);
            if (c->check.op[0] == '>')
                disparaAcao = (c->check.op[1] == '=')
                                  ? valorVar >= c->check.valor
                                  : valorVar > c->check.valor;
            else if (c->check.op[0] == '<')
                disparaAcao = (c->check.op[1] == '=')
                                  ? valorVar <= c->check.valor
                                  : valorVar < c->check.valor;
            else if (c->check.op[0] == '!')
                disparaAcao = valorVar != c->check.valor;
            else if (c->check.op[0] == '=')
                disparaAcao = valorVar == c->check.valor;
            else
            {
                logaM(LOG_CRITICO, "regrasProcessaEvento[%d] condicao.op (%s) DESCONHECIDA", regra->id, c->check.op);
                disparaAcao = false;
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

String regraGetTxt(Regra *r)
{
    String ret;

    ret.reserve(96);

    // Condição
    switch (r->condicao.tipo)
    {
    case COND_EVENTO:
        ret += "QUANDO ";
        ret += r->condicao.evento.recursoID;
        ret += ":";
        ret += regraTipoEventoTxt(r->condicao.evento.tipo);
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

    default:
        ret += "CONDICAO?";
        break;
    }
    if (r->condicao.check.variavel[0] != '\0')
    {
        ret += ", SE ";
        ret += r->condicao.check.variavel;
        ret += " ";
        ret += r->condicao.check.op;
        ret += " ";
        ret += r->condicao.check.valor;
    }

    ret += " -> ";

    // Ação
    switch (r->acao.tipo)
    {
    case ACAO_ESTADO:
        ret += r->acao.recursoID;
        ret += ":";
        ret += r->acao.comando;
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

JsonDocument regraGetCondicaoJSONDoc(Regra *r)
{
    JsonDocument doc;
    Condicao *c = &r->condicao;

    doc["tipo"] = regraTipoCondicaoTxt(c->tipo);
    switch (c->tipo)
    {
    case COND_EVENTO:
        doc["recurso"] = c->evento.recursoID;
        doc["evento"] = regraTipoEventoTxt(c->evento.tipo);
        break;

    case COND_HORARIO:
        doc["hora"] = c->horario.hora;
        doc["minuto"] = c->horario.minuto;
        break;

    default:
        logaM(LOG_AVISO, "regraGetCondicoesJSONDoc[%s] :: tipoCondicao[%d] invalido", r->nome, c->tipo);
    }

    if (c->check.variavel[0] != '\0')
    {
        JsonObject check = doc["check"].to<JsonObject>();
        check["variavel"] = c->check.variavel;
        check["operacao"] = c->check.op;
        check["valor"] = c->check.valor;
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
        doc["comando"] = a->comando;
        break;

    case ACAO_TIMER:
        doc["recurso"] = a->recursoID;
        doc["timer"] = a->timer;
        break;
    }

    return doc;
}

void regraGetJS(Regra *r, JsonObject &obj)
{
    obj["id"] = r->id;
    obj["nome"] = r->nome;
    obj["ativa"] = r->ativa;

    obj["descricao"] = regraGetTxt(r);

    obj["quando"] = regraGetCondicaoJSONDoc(r);
    obj["acao"] = regraGetAcaoJSONDoc(r);
}

void regrasGetJSONDoc(JsonDocument &doc, Regra *novaRegra)
{
    JsonArray regrasOut = doc.to<JsonArray>();

    int totRegras = regrasCount();
    for (int r = 0; r < totRegras; r++)
    {
        Regra *regra = regraGetPorIndice(r);
        if (regra->remover)
            continue;

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

String regraDeleteFromJSON(uint8_t *json)
{
    JsonDocument doc;
    if (utilLeJson("regraDeleteFromJSON", doc, json))
        return "JSON Invalido";

    int id = doc["id"].as<int>();
    doc.clear();

    Regra *regra = regraGet(id);
    if (!regra)
        return "Regra Invalida";

    // DEL
    regra->remover = true;

    // Le regra->remover
    String msg = regrasPersiste();
    if (msg != "OK")
        return msg;

    // Recarregar as regras
    msg = regrasLoad(REGRAS_PATH);
    if (msg != "OK")
        return "regrasLoad():" + msg;

    httpEnviaSSERefresh();

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

    if (!doc["nome"].isNull())
        strlcpy(regra->nome, doc["nome"].as<const char *>(), sizeof(regra->nome));

    if (!doc["ativa"].isNull())
        regra->ativa = doc["ativa"].as<bool>();

    // preencher condicao
    if (!doc["quando"].isNull())
    {
        Condicao *cond = &regra->condicao;
        String tipoCondicaoStr = doc["quando"]["tipo"].as<String>();
        if (tipoCondicaoStr == "EVENTO")
        {
            cond->tipo = COND_EVENTO;
            strlcpy(cond->evento.recursoID, doc["quando"]["recurso"].as<const char *>(), sizeof(cond->evento.recursoID));

            String eventoStr = doc["quando"]["evento"].as<String>();
            if (eventoStr == "TOGGLE")
                cond->evento.tipo = EVENTO_TOGGLE;
            else if (eventoStr == "CLICK")
                cond->evento.tipo = EVENTO_CLICK;
            else if (eventoStr == "LIGOU")
                cond->evento.tipo = EVENTO_LIGOU;
            else if (eventoStr == "DESLIGOU")
                cond->evento.tipo = EVENTO_DESLIGOU;
            else if (eventoStr == "DUPCLICK")
                cond->evento.tipo = EVENTO_DOUBLE_CLICK;
            else
            // TODO :: outros eventos
            {
                logaM(LOG_CRITICO, "TipoEvento %s ??? Inativando regra[%d]", eventoStr.c_str(), regra->id);
                regra->ativa = false;
            }
        }
        else if (tipoCondicaoStr == "HORARIO")
        {
            cond->tipo = COND_HORARIO;
            cond->horario.hora = doc["quando"]["hora"].as<int>();
            cond->horario.minuto = doc["quando"]["minuto"].as<int>();
        }
        else
        {
            logaM(LOG_CRITICO, "TipoCondicao %s ??? Inativando regra[%d]", tipoCondicaoStr.c_str(), regra->id);
            regra->ativa = false;
        }

        if (!doc["quando"]["check"].isNull() && !doc["quando"]["check"]["variavel"].isNull() && !doc["quando"]["check"]["operacao"].isNull())
        {
            strlcpy(cond->check.variavel, doc["quando"]["check"]["variavel"].as<const char *>(), sizeof(cond->check.variavel));
            strlcpy(cond->check.op, doc["quando"]["check"]["operacao"].as<const char *>(), sizeof(cond->check.op));
            cond->check.valor = doc["quando"]["check"]["valor"].as<int>();
        }
    }

    // preencher acao
    if (!doc["acao"].isNull())
    {
        String tipoAcaoStr = doc["acao"]["tipo"];
        if (tipoAcaoStr == "ESTADO")
        {
            regra->acao.tipo = ACAO_ESTADO;
            strlcpy(regra->acao.recursoID,
                    doc["acao"]["recurso"].as<const char *>(),
                    sizeof(regra->acao.recursoID));
            regra->acao.comando = doc["acao"]["comando"].as<String>();
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
