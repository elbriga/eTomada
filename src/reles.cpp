#include <Arduino.h>

#include "eTomada.h"
#include "hardwareProfile.h"
#include "loga.h"
#include "rele.h"
#include "mutex.h"
#include "display.h"
#include "http.h"
#include "recurso.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("RELE", nivel, fmt, ##__VA_ARGS__)

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

static Rele reles[MAX_RELES];

static int boardReleCount = 0;

void relesInit()
{
  logaM(LOG_NORMAL, "Inicializando Relés Locais");

  // Zerar tudo
  memset(reles, 0, sizeof(reles));

  // Verificar quantos reles temos
  boardReleCount = 0;
  for (int r = 0; r < MAX_RELES; r++)
  {
    ReleHW rHW = hardwareProfile.reles[r];
    if (rHW.pino == 255)
      break;
    boardReleCount++;
  }

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
  if (numRele < 1 || numRele > relesGetCount())
  {
    return NULL;
  }

  return &reles[numRele - 1];
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

bool releControla(Rele *rele, bool estado, String &msgOut)
{
  MutexLock lock(modeloMutex);
  if (!lock)
  {
    return "releControla: mutex timeout";
  }

  return releControlaLocked(rele, estado, msgOut);
}

bool releControlaLocked(Rele *rele, bool estado, String &msgOut)
{
  if (!rele)
  {
    msgOut = "releControlaLocked: Rele invalido";
    return false;
  }

  if (rele->pino == -1)
  {
    msgOut = "releControlaLocked: pino invalido";
    return false;
  }

  String ret = "";
  if (estado != rele->estado) // TODO :: remover esse if?
  {
    digitalWrite(rele->pino, rele->invertido ? !estado : estado);
    rele->estado = estado;

    char msg[40];
    snprintf(msg, sizeof(msg), "%s (rele %d, pino %d)", // TODO :: nome
             (estado ? "Ligando" : "Desligando"), rele->num, rele->pino);
    msgOut = msg;
  }

  return true;
}
