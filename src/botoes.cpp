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

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("BOTAO", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Botao botoes[MAX_BOTOES]; // TODO :: Alocação dinamica

static int boardBotaoCount = 0;

void botoesInit()
{
  logaM(LOG_NORMAL, "Inicializando Botões Locais");

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
    pinMode(botao->pino, INPUT_PULLUP);

    botao->estado = !digitalRead(botao->pino);
    botao->ultimoEstado = botao->estado;

    botao->debounce = millis();
    botao->ultimoToggle = millis();
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

// REQUIRE recursosMutex locked
JsonDocument botaoGetJSONDoc(Recurso *r, bool full)
{
  JsonDocument doc;
  Botao *b = recursoGetBotao(r);
  if (!b)
    return doc;

  doc["num"] = b->num;
  // TODO :: nome botao
  // doc["nome"] = b->nome;
  // doc["tipo"] = s->tipo;

  if (full)
  {
    doc["pino"] = b->pino;
    doc["estado"] = b->estado;
  }

  return doc;
}

void botoesAtualiza()
{
  if (!botoesGetCount())
    return;

  MutexLock lock(recursosMutex);
  if (!lock)
  {
    logaM(LOG_CRITICO, "botoesAtualiza: mutex timeout");
    return;
  }

  int totRecursos = recursosGetCount(), duracaoAnterior;
  for (int r = 0; r < totRecursos; r++)
  {
    Recurso *rec = recursoGetPorIndice(r);
    if (rec->tipo != RECURSO_BOTAO)
      continue;
    if (rec->remoto)
      continue;

    Botao *botao = rec->botao;

    if (botao->pino == -1)
    {
      // Desativado
      continue;
    }

    // Debounce
    bool leitura = !digitalRead(botao->pino); // PINO LOW == BOTAO ON

    if (leitura != botao->ultimoEstado)
    {
      botao->debounce = millis();
      botao->ultimoEstado = leitura;
    }

    uint32_t agora = millis();
    if (agora - botao->debounce > BOTAO_DEBOUCE_TIME_MS)
    {
      if (botao->estado != leitura)
      {
        logaM(LOG_NORMAL, "BOTAO [%s][%s] MUDOU [%s]",
              rec->id, rec->nome, leitura ? "ON" : "OFF");

        duracaoAnterior = agora - botao->ultimoToggle;

        botao->estado = leitura;
        botao->ultimoToggle = agora;

        eventoPost(botao->estado ? EVENTO_LIGOU : EVENTO_DESLIGOU, rec->id, true, true);
        eventoPost(EVENTO_TOGGLE, rec->id, true, true);

        // Detectar CLICK, em qualquer direcao
        if (duracaoAnterior < BOTAO_TEMPO_CLICK_MS)
          eventoPost(EVENTO_CLICK, rec->id, true, true);

        // Detectar longPress e bigPress ao desligar
        if (!botao->estado && duracaoAnterior > BOTAO_TEMPO_LONGP_MS)
          eventoPost(EVENTO_LONG_PRESS, rec->id, true, true);
      }
    }
  }
}
