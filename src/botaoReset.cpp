#include <Arduino.h>

#include "botaoReset.h"
#include "loga.h"
#include "hardwareProfile.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("BTNRST", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

typedef struct
{
  bool estado; // nível atual
  bool ultimoEstado;
  uint32_t debounce;
  uint32_t ultimoToggle; // para detectar CLICK
} BotaoReset;

static BotaoReset botaoReset = {};

void botaoResetInit()
{
  if (!botaoResetAtivo())
    return;

  logaM(LOG_NORMAL, "Ativando botao de reset em [%d]", hardwareProfile.btnResetPin);
  pinMode(hardwareProfile.btnResetPin, INPUT_PULLUP);

  botaoReset.estado = !digitalRead(hardwareProfile.btnResetPin);
  botaoReset.ultimoEstado = botaoReset.estado;
  botaoReset.debounce = 0;
  botaoReset.ultimoToggle = 0;
}

bool botaoResetAtivo()
{
  return hardwareProfile.btnResetPin != 255;
}

void botaoResetAtualiza()
{
  bool leitura = !digitalRead(hardwareProfile.btnResetPin); // PINO LOW == BOTAO ON

  if (leitura != botaoReset.ultimoEstado)
  {
    botaoReset.ultimoEstado = leitura;
    botaoReset.debounce = millis();
  }

  if ((uint32_t)(millis() - botaoReset.debounce) < BOTAO_DEBOUCE_TIME_MS)
    return;

  if (botaoReset.estado == leitura)
    return;

  uint32_t agora = millis();
  uint32_t duracaoAnterior = agora - botaoReset.ultimoToggle;

  botaoReset.estado = leitura;
  botaoReset.ultimoToggle = agora;

  logaM(LOG_NORMAL, "BOTAO RESET MUDOU [%s]", leitura ? "ON" : "OFF");

  // Detectar CLICK, em qualquer direcao
  if (duracaoAnterior < BOTAO_TEMPO_CLICK_MS)
    logaM(LOG_AVISO, "BTN RESET CLICK!");

  // Detectar longPress e bigPress ao desligar
  if (!botaoReset.estado)
  {
    if (duracaoAnterior > BOTAO_TEMPO_BIGP_MS)
      logaM(LOG_AVISO, "BTN RESET BIG PRESS!!");
    else if (duracaoAnterior > BOTAO_TEMPO_LONGP_MS)
      logaM(LOG_AVISO, "BTN RESET LONG PRESS!");
  }
}
