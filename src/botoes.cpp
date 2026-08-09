#include <Arduino.h>

#include "eTomada.h"
#include "mestre.h"
#include "hardwareProfile.h"
#include "botao.h"
#include "loga.h"
#include "http.h"
#include "mutex.h"
#include "prefs.h"
#include "recurso.h"
#include "eventos.h"

#define BOTAO_DEBOUCE_TIME_MS 50

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Botao botoes[MAX_BOTOES];

static int boardBotaoCount = 0;

// struct temporaria usada em botoesAtualiza
struct AtualizacaoBotao
{
  Recurso *rec;
  int novoEstado;
};

void botoesInit()
{
  // Zerar tudo
  memset(botoes, 0, sizeof(botoes));

  // Verificar quantos botoes temos
  boardBotaoCount = 0;
  for (int b = 0; b < MAX_BOTOES; b++)
  {
    BotaoHW bHW = hardwareProfile.botoes[b];
    if (bHW.pino == 255)
      break;
    boardBotaoCount++;
  }

  Preferences prefs;
  prefs.begin("botoes", false);

  int totBotoes = botoesGetCount();
  for (int b = 1; b <= totBotoes; b++)
  {
    Botao *botao = botaoGet(b);
    botao->num = b;

    BotaoHW bHW = hardwareProfile.botoes[b - 1];

    botao->pino = bHW.pino;

    botao->ativo = (botao->pino != 255);
    if (botao->ativo)
    {
      pinMode(botao->pino, INPUT);
    }

    botao->estado = digitalRead(botao->pino);

    botaoPrint(botao);
  }

  prefs.end();
}

int botoesGetCount()
{
  return boardBotaoCount;
}

Botao *botaoGet(int num)
{
  if (num > 0 && num <= botoesGetCount())
  {
    return &botoes[num - 1];
  }

  return NULL;
}

void botaoPrint(Botao *botao)
{
  logaMensagem("Botao %d:%d (%s)",
               botao->num, botao->pino,
               (botao->ativo ? "on" : "off"));
}

// REQUIRE recursosMutex locked
JsonDocument botaoGetJSONDoc(Botao *b, bool full)
{
  JsonDocument doc;

  doc["num"] = b->num;
  // TODO :: nome botao
  // doc["nome"] = b->nome;
  // doc["tipo"] = s->tipo;

  if (full)
  {
    doc["pino"] = b->pino;
    doc["estado"] = b->estado;
    doc["ativo"] = b->ativo;
  }

  return doc;
}

// REQUIRE recursosMutex locked
String botaoGetJSONString(Botao *b)
{
  String out;
  JsonDocument doc = botaoGetJSONDoc(b, true);

  serializeJson(doc, out);
  return out;
}

void botoesAtualiza()
{
  int maxBotoes = botoesGetCount();
  if (!maxBotoes)
    return;

  int totRecursos = recursosGetCount(RECURSO_TODOS);

  AtualizacaoBotao *atual = new AtualizacaoBotao[maxBotoes]();

  // Ler os botoes sem o Lock
  int totBotoesParaAtualizar = 0;
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (rec->tipo != RECURSO_BOTAO)
      continue;
    if (rec->remoto)
      continue;

    Botao *botao = rec->botao;

    if (!botao->ativo || botao->pino == -1)
    {
      // Desativado
      continue;
    }

    // Debounce
    bool leitura = digitalRead(botao->pino);

    if (leitura != botao->ultimoEstado)
    {
      botao->debounce = millis();
      botao->ultimoEstado = leitura;
    }

    if (millis() - botao->debounce > BOTAO_DEBOUCE_TIME_MS)
    {
      if (botao->estado != leitura)
      {
        logaMensagem("BOTAO MUDOU!!!!!!! [%s]", leitura ? "ON" : "OFF");
        int idx = totBotoesParaAtualizar++;
        atual[idx].rec = rec;
        atual[idx].novoEstado = leitura;
      }
    }
  }

  // Atualizar os recursos BOTAO COM LOCK
  {
    MutexLock lock(recursosMutex);
    if (!lock)
    {
      delete[] atual;
      logaMensagem("botoesAtualiza: mutex timeout");
      return;
    }

    for (int rb = 0; rb < totBotoesParaAtualizar; rb++)
    {
      Recurso *rec = atual[rb].rec;
      Botao *botao = rec->botao;

      botao->estado = atual[rb].novoEstado;
    }
  }

  // Enviar os Eventos e os SSE sem Lock
  for (int rb = 0; rb < totBotoesParaAtualizar; rb++)
  {
    Recurso *rec = atual[rb].rec;
    Botao *botao = rec->botao;
    // recursoEnviaSSE(atual[rb].rec) e mestreEnviaEvento(atual[rb].rec) em outra thread
    eventoPost(botao->estado ? EVENTO_LIGOU : EVENTO_DESLIGOU, atual[rb].rec, true, true);
    eventoPost(EVENTO_TOGGLE, atual[rb].rec, true, true);
  }

  delete[] atual;
}
