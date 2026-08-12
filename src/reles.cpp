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
    rele->override = 0;

    ReleHW rHW = hardwareProfile.reles[r - 1];
    rele->pino = rHW.pino;
    rele->ativo = (rele->pino >= 0); // TODO > != 255 ?
    rele->invertido = rHW.invertido;

    if (rele->ativo)
    {
      pinMode(rele->pino, OUTPUT);
      digitalWrite(rele->pino, rele->invertido ? !rele->estado : rele->estado);
    }

    relePrint(rele);
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

void relePrint(Rele *rele)
{
  logaM(LOG_NORMAL, "Rele %d:%d (%s)",
        rele->num, rele->pino, // TODO :: nome
        (rele->ativo ? "on" : "off"));
}

// REQUIRE releMutex locked
JsonDocument releGetJSONDoc(Rele *r, bool full)
{
  JsonDocument doc;

  doc["num"] = r->num;

  if (full)
  {
    doc["pino"] = r->pino;
    doc["ativo"] = r->ativo;
    doc["estado"] = r->estado;
    doc["override"] = r->override;
  }

  return doc;
}

// REQUIRE releMutex locked
String releGetJSONString(Rele *r)
{
  String out;
  JsonDocument doc = releGetJSONDoc(r, true);

  serializeJson(doc, out);
  return out;
}

String releControla(Rele *rele, bool estado, int override)
{
  MutexLock lock(recursosMutex);
  if (!lock)
  {
    return "releControla: mutex timeout";
  }

  return releControlaLocked(rele, estado, override);
}

// REQUIRE recursosMutex locked
String releControlaLocked(Rele *rele, bool estado, int override)
{
  if (!rele)
  {
    logaM(LOG_CRITICO, "releControlaLocked: Rele invalido!!!\n");
    // TODO :: um metodo retorna erro e outro vazio! REVER
    return "";
  }

  if (rele->pino == -1)
  {
    logaM(LOG_AVISO, "releControlaLocked[%d]: pino invalido!\n", rele->num);
    return "";
  }

  String ret = "";
  if (estado != rele->estado)
  {
    digitalWrite(rele->pino, rele->invertido ? !estado : estado);
    rele->estado = estado;

    rele->override = (override > 0) ? time(nullptr) + override : 0;

    char msg[40];
    snprintf(msg, sizeof(msg), "%s (rele %d, pino %d)", // TODO :: nome
             (estado ? "Ligando" : "Desligando"), rele->num, rele->pino);
    ret = msg;
  }

  return ret;
}
