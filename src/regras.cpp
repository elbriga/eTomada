#include <memory>
#include <new>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "mutex.h"
#include "ntp.h"
#include "util.h"
#include "recurso.h"
#include "agendamentos.h"
#include "sensorChuva.h"
#include "http.h"
#include "led.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("REGRA", nivel, fmt, ##__VA_ARGS__)

#define MAX_REGRAS 64
#define REGRAS_TMP "/automacoes.json.tmp"
#define REGRAS_BAK "/automacoes.json.bak"

int regrasTotal = 0;
Regra *regras = nullptr;

static char regrasRevision[65] = {};

String regrasLoad(const char *path);
String regraGetTxt(Regra *r);
void regraLoadFromJSON(Regra *regra, JsonObject &doc);
String regrasPersiste(Regra *novaRegra = nullptr);

void regrasInit()
{
    // Boot > recuperar regras anteriores se a substituição foi interrompida.
    if (!LittleFS.exists(REGRAS_PATH) && LittleFS.exists(REGRAS_BAK))
    {
        if (LittleFS.rename(REGRAS_BAK, REGRAS_PATH))
            logaM(LOG_AVISO, "Arquivo de regras RECUPERADO do BAK");
        else
            logaM(LOG_CRITICO, "Falha ao recuperar automacoes.json.bak");
    }

    // Sem configuração local > aguardar sincronização do Server.
    if (!LittleFS.exists(REGRAS_PATH))
    {
        logaM(LOG_AVISO, "Sem regras locais. Aguardando Server.");
        regrasRevision[0] = '\0';
        return;
    }

    String msg = regrasLoad(REGRAS_PATH);
    if (msg != "OK")
        logaM(LOG_AVISO, ">> regrasLoad: [%s]", msg.c_str());
    else
    {
        // Buscar a revisão
        File arquivo = LittleFS.open(REGRAS_PATH, "r");
        JsonDocument doc;
        if (arquivo && !deserializeJson(doc, arquivo))
        {
            const char *revision = doc["revision"] | "";
            strlcpy(regrasRevision, revision, sizeof(regrasRevision));
        }
        else
            regrasRevision[0] = '\0';

        if (arquivo)
            arquivo.close();
    }
}

Regra *regrasCalculaEstadoAtual(Recurso *recursoIn, String &estadoAtualOut)
{
    // Tratando somente reles por enquanto
    if (recursoIn->tipo != RECURSO_RELE)
        return nullptr;

    struct tm timeinfo;
    sysGetTime(&timeinfo);
    int minutoAtual = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int menorDistancia = 1441;

    Regra *regraAtivadaOut = nullptr;
    for (int r = 0; r < regrasTotal; r++)
    {
        Regra *regra = &regras[r];
        if (!regra->ativa || regra->remover)
            continue;

        // Trabalhar com regras de HORARIO
        Condicao *cond = &regra->condicao;
        if (cond->tipo != COND_HORARIO)
            continue;

        // Trabalhar em cima de ON e OFF
        if (strcmp(regra->acao.comando, "ON") && strcmp(regra->acao.comando, "OFF"))
            continue;

        // Verificar se esta regra age em cima do recurso
        if (strcmp(regra->acao.recursoID, recursoIn->id))
            continue;

        // Verificar se ja passou esse HORARIO
        int minutoRegra = cond->horario.hora * 60 + cond->horario.minuto;
        // Boot > considera também a última regra do dia anterior.
        int distancia = (minutoAtual - minutoRegra + 1440) % 1440;

        if (distancia < menorDistancia ||
            (distancia == menorDistancia &&
             regraAtivadaOut && regra->id < regraAtivadaOut->id))
        {
            menorDistancia = distancia;
            regraAtivadaOut = regra;
            estadoAtualOut = regra->acao.comando;
        }
    }

    return regraAtivadaOut;
}

#define REGRAS_BOOT_MAX_ACOES 16
typedef struct
{
    String recursoID, estado;
} CacheAcao;
static bool regrasBootOK = false;
bool regrasBoot(const char *nodoID)
{
    if (regrasBootOK && !nodoID) // Rodar o "all" somente uma vez, mas permitir boot por nodo
        return true;

    // Obter horario
    struct tm timeinfo;
    sysGetTime(&timeinfo);
    if (timeinfo.tm_year + 1900 < 2026)
    {
        // Sem data/hora não processa regras de HORARIO
        logaM(LOG_AVISO, "Pulando Boot das regras!!! estamos sem HORA!!");
        return false;
    }

    int totAcoes = 0;
    CacheAcao cache[REGRAS_BOOT_MAX_ACOES] = {};

    {
        MutexLock lock(modeloMutex, "regrasBoot");
        if (!lock)
            return false;

        logaM(LOG_DEBUG0, "== regrasBoot(%s) ==", nodoID ? nodoID : "all");

        Regra *regraAlerta = regraTemAlerta();
        if (regraAlerta && regraAlerta->condicao.tipo == COND_EVENTO)
        {
            Recurso *r = recursoGet(regraAlerta->condicao.evento.recursoID);
            if (r)
            {
                bool alerta = recursoGetValor(r);
                ledSetAnim(alerta ? LED_ANIM_ALARM : LED_ANIM_OFF);
            }
        }

        // Ajustar o estado dos RELEs conforme as regras de HORARIO para agora
        bool somenteNodo = nodoID && strlen(nodoID) > 0;
        int totRecursos = recursosGetCount();
        for (int r = 0; r < totRecursos; r++)
        {
            Recurso *recurso = recursoGetPorIndice(r);
            if (recurso->tipo != RECURSO_RELE) // TODO Umid?
                continue;

            if (somenteNodo) // Verificar as regras somente de 1 nodo se informado
            {
                if (!recurso->remoto)
                    continue;
                if (strcmp(recurso->recursoRemoto->nodo->id, nodoID))
                    continue;
            }

            String estadoAtual;
            Regra *regraAtivada = regrasCalculaEstadoAtual(recurso, estadoAtual);
            if (regraAtivada)
            {
                logaM(LOG_NORMAL, "Setar recurso [%s][%s] para [%s] pela regra [%s]",
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
    String msg;
    bool ret = true;
    for (int c = 0; c < totAcoes; c++)
    {
        if (!recursoSet(cache[c].recursoID.c_str(), cache[c].estado.c_str(), msg))
        {
            logaM(LOG_AVISO, "regrasBoot > recursoSet > [%s]", msg.c_str());
            ret = false;
        }
    }

    if (ret && !nodoID)
        regrasBootOK = true;

    return ret;
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

Regra *regraTemAlerta()
{
    int tot = regrasCount();
    for (int r = 0; r < tot; r++)
    {
        Regra *regra = &regras[r];
        if (!strcmp(regra->acao.recursoID, "__ALERTA__"))
            return regra;
    }

    return nullptr;
}

#define REGRAS_MAX_ACOES_PENDENTES 8
struct AcaoPendente
{
    int regraID;
    char regraTxt[128];
    char recursoID[32];
    char comando[16];
    int timerOff;
    bool enviaEventos;
};

bool acaoPendenteAdiciona(int &count, AcaoPendente *acoes, Regra *regra, const char *recursoID, const char *comando, int timerOff = 0, bool enviaEventos = true)
{
    if (count >= REGRAS_MAX_ACOES_PENDENTES)
    {
        logaM(LOG_CRITICO, "Cache de Acoes Pendentes de regras [ESTOUROU!]");
        return false;
    }

    acoes[count].regraID = regra->id;
    strlcpy(acoes[count].regraTxt, regraGetTxt(regra).c_str(), sizeof(acoes[count].regraTxt));
    strlcpy(acoes[count].recursoID, recursoID, sizeof(acoes[count].recursoID));
    strlcpy(acoes[count].comando, comando, sizeof(acoes[count].comando));
    acoes[count].timerOff = timerOff;
    acoes[count].enviaEventos = enviaEventos;

    count++;

    return true;
}

String regraAddAcaoCache(Regra *regra, int &count, AcaoPendente *acoes)
{
    Acao *acao = &regra->acao;
    logaM(LOG_NORMAL, ">> Ativando [%s]", regraGetTxt(regra).c_str());

    String msg;
    switch (acao->tipo)
    {
    case ACAO_ESTADO:
    {
        if (!strcmp(acao->recursoID, "__ALERTA__"))
        {
            // TODO :: Aterta esta sempre ignorando o comando e fazendo FOLLOW
            Recurso *recAlerta = recursoGet(regra->condicao.evento.recursoID);
            if (!recAlerta)
                return "dispAcaoESTADO : Origem invalida?1";
            int estado = recursoGetValor(recAlerta);
            // ledSetAnim(estado ? LED_ANIM_ALARM : LED_ANIM_OFF);
            if (!acaoPendenteAdiciona(count, acoes, regra, "__ALERTA__", estado ? "ON" : "OFF"))
                return "Erro add AP";
            break;
        }

        Recurso *rec = recursoGet(acao->recursoID);
        if (!rec || rec->tipo != RECURSO_RELE)
            return "dispAcaoESTADO : Nao eh RELE!";

        String comando = acao->comando;
        if (!strcmp(acao->comando, "FOLLOW"))
        {
            Recurso *recOrigem = recursoGet(regra->condicao.evento.recursoID);
            if (!recOrigem)
                return "dispAcaoESTADO FOLLOW : Origem invalida?2";
            int estadoOrigem = recursoGetValor(recOrigem);
            comando = estadoOrigem ? "ON" : "OFF";
        }
        else if (!strcmp(acao->comando, "TOGGLEF"))
        {
            bool estadoDestino = !recursoGetValor(rec);
            comando = estadoDestino ? "ON" : "OFF";

            Recurso *recOrigem = recursoGet(regra->condicao.evento.recursoID);
            if (!recOrigem)
                return "dispAcaoESTADO FOLLOW : Origem invalida?3";
            bool estadoOrigem = recursoGetValor(recOrigem);
            if (estadoOrigem != estadoDestino)
            {
                // Corrigir o estado do chamador sem gerar eventos
                // String msg2;
                // recursoSet(regra->condicao.evento.recursoID, comando, msg2, false, false);
                if (count < REGRAS_MAX_ACOES_PENDENTES - 1)
                    acaoPendenteAdiciona(count, acoes, regra, regra->condicao.evento.recursoID, comando.c_str(), 0, false);
                else
                    logaM(LOG_CRITICO, "regraAddAcaoCache > TOGGLEF > sem espaço do cache de Acoes para Folow");
            }
        }

        // if (!recursoSet(acao->recursoID, comando, msg))
        //     return msg;
        if (!acaoPendenteAdiciona(count, acoes, regra, acao->recursoID, comando.c_str()))
            return "Erro add AP 3";
    }
    break;

    case ACAO_TIMER:
    {
        Recurso *rec = recursoGet(acao->recursoID);
        if (!rec || rec->tipo != RECURSO_RELE)
            return "dispAcaoTIMER : Nao eh RELE!";

        // if (!recursoSet(acao->recursoID, "ON", msg))
        //     return msg;

        // // Agendar o OFF
        // agendamentosAdd(AGEND_RECURSO, acao->timer * 1000, acao->recursoID, "OFF");

        if (!acaoPendenteAdiciona(count, acoes, regra, acao->recursoID, "ON", acao->timer))
            return "Erro add AP TIMER";
    }
    break;

    default:
        logaM(LOG_CRITICO, "TODO :: regraDispara[%d] tipo (%d)", regra->id, acao->tipo);
        return "TODO";
    }

    return "OK";
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
    // Limpar o cache
    int acoesPendentesCount = 0;
    AcaoPendente acoesPendentes[REGRAS_MAX_ACOES_PENDENTES] = {};

    // Processar o evento dentro do lock
    {
        MutexLock lock(modeloMutex, "regrasProcessaEvento");
        if (!lock)
            return;

        for (int r = 0; r < regrasTotal; r++)
        {
            Regra *regra = &regras[r];

            if (!regra->ativa || regra->remover)
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
                String msg = regraAddAcaoCache(regra, acoesPendentesCount, acoesPendentes);
                if (msg != "OK")
                    logaM(LOG_AVISO, "ERRO > regraProcessaEvento > addAcaoCache > [%s]", msg.c_str());
            }
        }
    }

    // Executar fora do Lock
    if (acoesPendentesCount > 0)
    {
        String msg;
        for (int i = 0; i < acoesPendentesCount; i++)
        {
            AcaoPendente *ap = &acoesPendentes[i];

            // Verificar se regraTxt é igual - usar ID como chave
            {
                MutexLock lock(modeloMutex, "regrasProcessaEvento2");
                if (!lock)
                    return;

                Regra *regra = regraGet(ap->regraID);
                if (!regra || !regra->ativa || regra->remover)
                {
                    logaM(LOG_AVISO, "Regra [%d] removida ou desativada!", ap->regraID);
                    continue;
                }

                String txtNovo = regraGetTxt(regra);
                if (txtNovo != ap->regraTxt)
                {
                    logaM(LOG_AVISO, "Regra [%d] mudou enquanto estavamos fora!", ap->regraID);
                    continue;
                }
            }

            if (!recursoSet(ap->recursoID, ap->comando, msg, ap->enviaEventos, ap->enviaEventos))
            {
                logaM(LOG_AVISO, "ERRO regraProcessaEvento > [%s]", msg.c_str());
                continue;
            }

            if (ap->timerOff > 0)
            {
                // Agendar o OFF
                agendamentosAdd(AGEND_RECURSO, ap->timerOff * 1000, ap->recursoID, "OFF");
            }

            logaM(LOG_NORMAL, "Resultado da Regra[%s]: [%s]", ap->regraTxt, msg.c_str());
        }
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

    if (!LittleFS.rename("/automacoes.json.tmp", "/automacoes.json"))
        return "ERRO: regrasPersiste:Rename";

    logaM(LOG_NORMAL, "Regras Salvas!");
    return "OK";
}

// Validação > rejeitar regras incompatíveis antes de modificar o modelo.
static bool regraSyncValida(JsonObject regra)
{
    int id = regra["id"] | 0;
    const char *nome = regra["nome"] | "";
    JsonObject quando = regra["quando"].as<JsonObject>();
    JsonObject acao = regra["acao"].as<JsonObject>();

    if (id < 1 || id > 65535 ||
        strlen(nome) == 0 || strlen(nome) >= 64 ||
        !regra["ativa"].is<bool>() ||
        quando.isNull() || acao.isNull())
        return false;

    const char *tipoCond = quando["tipo"] | "";

    if (!strcmp(tipoCond, "HORARIO"))
    {
        if (!quando["hora"].is<int>() || !quando["minuto"].is<int>())
            return false;

        int hora = quando["hora"];
        int minuto = quando["minuto"];

        if (hora < 0 || hora > 23 || minuto < 0 || minuto > 59)
            return false;
    }
    else if (!strcmp(tipoCond, "EVENTO"))
    {
        const char *recurso = quando["recurso"] | "";
        const char *evento = quando["evento"] | "";

        if (!*recurso || strlen(recurso) >= 32 ||
            !recursoGet(recurso))
            return false;

        if (strcmp(evento, "TOGGLE") &&
            strcmp(evento, "CLICK") &&
            strcmp(evento, "DUPCLICK") &&
            strcmp(evento, "LIGOU") &&
            strcmp(evento, "DESLIGOU"))
            return false;
    }
    else
        return false;

    if (!quando["check"].isNull())
    {
        JsonObject check = quando["check"].as<JsonObject>();
        const char *variavel = check["variavel"] | "";
        const char *op = check["operacao"] | "";

        if (!*variavel || strlen(variavel) >= 32 ||
            (strcmp(variavel, "HORASSECO") && !recursoGet(variavel)) ||
            !check["valor"].is<int>())
            return false;

        if (strcmp(op, ">") && strcmp(op, "<") &&
            strcmp(op, "=") && strcmp(op, "!=") &&
            strcmp(op, ">=") && strcmp(op, "<="))
            return false;
    }

    const char *tipoAcao = acao["tipo"] | "";
    const char *destino = acao["recurso"] | "";

    if (!*destino || strlen(destino) >= 32)
        return false;

    if (strcmp(destino, "__ALERTA__"))
    {
        Recurso *recurso = recursoGet(destino);
        if (!recurso || recurso->tipo != RECURSO_RELE || recurso->remoto)
            return false;
    }

    if (!strcmp(tipoAcao, "ESTADO"))
    {
        const char *cmd = acao["comando"] | "";

        return !strcmp(cmd, "ON") ||
               !strcmp(cmd, "OFF") ||
               !strcmp(cmd, "TOGGLE") ||
               !strcmp(cmd, "FOLLOW") ||
               !strcmp(cmd, "TOGGLEF");
    }

    if (!strcmp(tipoAcao, "TIMER"))
    {
        return strcmp(destino, "__ALERTA__") &&
               acao["timer"].is<uint32_t>() &&
               acao["timer"].as<uint32_t>() > 0;
    }

    return false;
}

String regrasGetRevision()
{
    return String(regrasRevision);
}

#define REGRAS_SYNC_MAX_ESTADOS 16

struct EstadoHorarioSync
{
    char recursoID[32] = {};
    char estado[4] = {};
};

// Captura o estado esperado dos relés locais pelas regras de horário.
// Deve ser chamada com modeloMutex adquirido.
static size_t regrasCapturaEstadosHorario(EstadoHorarioSync *saida, size_t capacidade)
{
    struct tm horario;
    sysGetTime(&horario);

    if (horario.tm_year + 1900 < 2026)
        return 0;

    size_t total = 0;

    for (int i = 0; i < recursosGetCount(); i++)
    {
        Recurso *recurso = recursoGetPorIndice(i);

        if (!recurso ||
            recurso->tipo != RECURSO_RELE ||
            recurso->remoto)
            continue;

        if (total >= capacidade)
        {
            logaM(LOG_CRITICO, "Limite de estados na sincronizacao");
            break;
        }

        EstadoHorarioSync &item = saida[total++];

        strlcpy(item.recursoID, recurso->id,
                sizeof(item.recursoID));

        String estado;
        if (regrasCalculaEstadoAtual(recurso, estado))
            strlcpy(item.estado, estado.c_str(),
                    sizeof(item.estado));
    }

    return total;
}

int regrasSubstituir(JsonDocument &pacote, String &msg)
{
    const char *deviceId = pacote["device_id"] | "";
    const char *revision = pacote["revision"] | "";
    JsonArray lista = pacote["regras"].as<JsonArray>();

    if (!*deviceId ||
        !String(deviceId).equalsIgnoreCase(eTomadaDeviceID()) ||
        strlen(revision) != 64 ||
        lista.isNull() || lista.size() > MAX_REGRAS)
    {
        msg = "Pacote de regras invalido";
        return 422;
    }

    for (int i = 0; i < 64; i++)
    {
        char c = revision[i];
        if (!isxdigit((unsigned char)c))
        {
            msg = "Revision invalida";
            return 422;
        }
    }

    const size_t total = lista.size();

    std::unique_ptr<Regra[]> novas(total ? new (std::nothrow) Regra[total]() : nullptr);
    // Remove o ERROR do vscode, mas é pior
    // std::unique_ptr<Regra[]> novas(total ? new Regra[total]() : nullptr);

    if (total && !novas)
    {
        msg = "Memoria insuficiente";
        return 507;
    }

    MutexLock configLock(configMutex, "regrasSync config");
    if (!configLock)
    {
        msg = "Config mutex timeout";
        return 503;
    }

    {
        MutexLock lock(modeloMutex, "regrasSync valida");
        if (!lock)
        {
            msg = "Modelo mutex timeout";
            return 503;
        }

        // Valida e monta o novo array sem modificar as regras ativas.
        for (size_t i = 0; i < total; i++)
        {
            JsonObject obj = lista[i].as<JsonObject>();

            if (obj.isNull() || !regraSyncValida(obj))
            {
                msg = "Regra invalida na posicao " + String(i);
                return 422;
            }

            for (size_t j = 0; j < i; j++)
            {
                if (novas[j].id == obj["id"].as<int>())
                {
                    msg = "ID de regra duplicado";
                    return 422;
                }
            }

            regraLoadFromJSON(&novas[i], obj);
        }
    }

    // Persistência > grava e verifica o arquivo temporário.
    LittleFS.remove(REGRAS_TMP);

    File arquivo = LittleFS.open(REGRAS_TMP, "w");
    if (!arquivo)
    {
        msg = "Erro ao abrir arquivo temporario";
        return 500;
    }

    size_t esperado = measureJson(pacote);
    size_t gravado = serializeJson(pacote, arquivo);

    arquivo.flush();
    arquivo.close();

    if (gravado != esperado)
    {
        LittleFS.remove(REGRAS_TMP);
        msg = "Gravacao incompleta";
        return 507;
    }

    File leitura = LittleFS.open(REGRAS_TMP, "r");
    JsonDocument verificacao;

    bool arquivoOK = leitura &&
                     !deserializeJson(verificacao, leitura);

    if (leitura)
        leitura.close();

    if (arquivoOK)
    {
        arquivoOK =
            verificacao["revision"].as<String>() == revision &&
            verificacao["regras"].as<JsonArray>().size() == total &&
            measureJson(verificacao) == esperado;
    }

    if (!arquivoOK)
    {
        LittleFS.remove(REGRAS_TMP);
        msg = "Falha na verificacao do arquivo";
        return 500;
    }

    EstadoHorarioSync antes[REGRAS_SYNC_MAX_ESTADOS] = {};
    EstadoHorarioSync depois[REGRAS_SYNC_MAX_ESTADOS] = {};

    size_t totalAntes = 0;
    size_t totalDepois = 0;

    {
        MutexLock lock(modeloMutex, "regrasSync aplica");
        if (!lock)
        {
            LittleFS.remove(REGRAS_TMP);
            msg = "Modelo mutex timeout";
            return 503;
        }

        // Substituição > preserva o arquivo antigo para rollback.
        if (LittleFS.exists(REGRAS_BAK) &&
            !LittleFS.remove(REGRAS_BAK))
        {
            msg = "Nao foi possivel limpar backup antigo";
            return 500;
        }

        bool tinhaArquivo = LittleFS.exists(REGRAS_PATH);

        if (tinhaArquivo &&
            !LittleFS.rename(REGRAS_PATH, REGRAS_BAK))
        {
            msg = "Nao foi possivel preservar regras antigas";
            return 500;
        }

        if (!LittleFS.rename(REGRAS_TMP, REGRAS_PATH))
        {
            if (tinhaArquivo &&
                !LittleFS.rename(REGRAS_BAK, REGRAS_PATH))
            {
                logaM(LOG_CRITICO, "Falha no rollback de automacoes!");
            }

            msg = "Nao foi possivel instalar novo arquivo";
            return 500;
        }

        // Captura o estado esperado com as regras anteriores.
        totalAntes = regrasCapturaEstadosHorario(antes, REGRAS_SYNC_MAX_ESTADOS);

        // Modelo > troca o array somente depois da persistência.
        Regra *antigas = regras;
        regras = novas.release();
        regrasTotal = total;
        strlcpy(regrasRevision, revision, sizeof(regrasRevision));

        // Captura o estado esperado com as novas regras.
        totalDepois = regrasCapturaEstadosHorario(depois, REGRAS_SYNC_MAX_ESTADOS);

        delete[] antigas;
    }

    LittleFS.remove(REGRAS_BAK);

    // Reconciliação > aplicar somente estados esperados que mudaram.
    for (size_t i = 0; i < totalDepois; i++)
    {
        const EstadoHorarioSync &novo = depois[i];

        if (!novo.estado[0])
            continue;

        const char *anterior = "";
        for (size_t j = 0; j < totalAntes; j++)
        {
            if (!strcmp(antes[j].recursoID, novo.recursoID))
            {
                anterior = antes[j].estado;
                break;
            }
        }

        if (!strcmp(anterior, novo.estado))
            continue;

        String msg;
        if (!recursoSet(novo.recursoID, novo.estado, msg))
        {
            logaM(LOG_AVISO, "SYNC horario [%s] -> [%s]: %s",
                  novo.recursoID, novo.estado, msg.c_str());
        }
    }

    logaM(LOG_NORMAL, "Sincronizadas %u regras, revision %.12s",
          (unsigned)total, regrasRevision);

    msg = "OK";

    httpEnviaSSERefresh();
    return 200;
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
            strlcpy(regra->acao.recursoID, doc["acao"]["recurso"].as<const char *>(), sizeof(regra->acao.recursoID));
            strlcpy(regra->acao.comando, doc["acao"]["comando"].as<const char *>(), sizeof(regra->acao.comando));
        }
        else if (tipoAcaoStr == "TIMER")
        {
            regra->acao.tipo = ACAO_TIMER;
            strlcpy(regra->acao.recursoID, doc["acao"]["recurso"].as<const char *>(), sizeof(regra->acao.recursoID));
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

    if (regrasJson.isNull() || regrasJson.size() > MAX_REGRAS)
        return "ERRO: conjunto de regras invalido";

    const size_t total = regrasJson.size();

    // Carregamento > montar tudo antes de substituir o array ativo.
    std::unique_ptr<Regra[]> novas(total ? new (std::nothrow) Regra[total]() : nullptr);

    if (total && !novas)
        return "ERRO: memoria insuficiente";

    bool pacoteServidor = doc["revision"].is<const char *>();

    size_t carregadas = 0;

    for (JsonObject obj : regrasJson)
    {
        if (obj.isNull() || (pacoteServidor && !regraSyncValida(obj)))
            return "ERRO: regra invalida no arquivo";

        regraLoadFromJSON(&novas[carregadas], obj);
        carregadas++;
    }

    // Modelo > substituição somente após carregamento completo.
    // Esta função já é chamada sob modeloMutex em eTomadaLoadConfig().
    Regra *antigas = regras;
    regras = novas.release();
    regrasTotal = carregadas;

    delete[] antigas;

    return "OK";
}

void regraPrint(Regra *r)
{
    logaM(LOG_NORMAL, "Regra[%d][%s][%s] > [%s]",
          r->id, r->ativa ? "ON" : "OFF", r->nome,
          regraGetTxt(r).c_str());
}
