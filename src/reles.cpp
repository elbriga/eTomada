#include <Arduino.h>

#include "eTomada.h"
#include "hardwareProfile.h"
#include "loga.h"
#include "rele.h"
#include "mutex.h"
#include "display.h"
#include "http.h"
#include "recurso.h"
#include "util.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RELE", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Rele *reles = NULL;
static int boardReleCount = 0;

void relesInit()
{
  logaM(LOG_NORMAL, "Inicializando Relés Locais");

  // Verificar quantos reles temos
  boardReleCount = 0;
  for (int r = 0; r < MAX_RELES; r++)
  {
    ReleHW rHW = hardwareProfile.reles[r];
    if (rHW.pino == 255)
      break;
    boardReleCount++;
  }

  reles = new Rele[boardReleCount]();
  if (!reles)
    utilDIE("Nao foi possivel alocar memoria para reles");

  int totReles = relesGetCount();
  for (int r = 1; r <= totReles; r++)
  {
    Rele *rele = releGet(r);
    rele->num = r;

    // TODO :: guardar estado dos reles ativos e sem regra (modo manual) para voltar ao estado certo no boot
    rele->estado = 0;

    ReleHW rHW = hardwareProfile.reles[r - 1];
    rele->pino = rHW.pino;
    rele->invertido = rHW.invertido;

    pinMode(rele->pino, OUTPUT);
    digitalWrite(rele->pino, rele->invertido ? !rele->estado : rele->estado);
  }
}

int relesGetCount()
{
  return boardReleCount;
}

Rele *releGet(int numRele)
{
  return (numRele > 0 && numRele <= relesGetCount())
             ? &reles[numRele - 1]
             : NULL;
}

// REQUIRE releMutex locked
JsonDocument releGetJSONDoc(Recurso *rec, bool full)
{
  JsonDocument doc;
  Rele *r = recursoGetRele(rec);
  if (!r)
    return doc;

  doc["num"] = r->num;

  if (full)
  {
    doc["pino"] = r->pino;
    doc["estado"] = r->estado;
  }

  return doc;
}

bool releControla(Recurso *r, bool estado, String &msgOut)
{
  if (r->tipo != RECURSO_RELE)
  {
    msgOut = "releControla: Recurso não é Rele!";
    return false;
  }

  Rele *rele = recursoGetRele(r);
  if (!rele)
  {
    msgOut = "releControla: Rele invalido";
    return false;
  }

  if (rele->pino == -1)
  {
    msgOut = "releControla: pino invalido";
    return false;
  }

  if (estado == rele->estado) // TODO :: remover esse if?
  {
    msgOut = "Rele " + String(r->id) + " já " + (estado ? "Ligado" : "Desligado");
    return false;
  }

  digitalWrite(rele->pino, rele->invertido ? !estado : estado);
  rele->estado = estado;

  msgOut = (estado ? "Ligando" : "Desligando") + String(" Rele ") + r->id;
  return true;
}
