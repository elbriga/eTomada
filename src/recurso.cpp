#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "recurso.h"
#include "nodoRemoto.h"

Recurso *recursos;
static int totRecursos = 0;
static int totRecursosRemotos = 0;

void recursosInit() {
  int recursoAddCount = 0;
  int totRelesLocais = relesGetCount();
  int totSensoresLocais = sensoresGetCount();

  Preferences prefs;
  prefs.begin("recursosRemotos", false);

  totRecursosRemotos = getPrefsAtr(prefs, 0, "total").toInt();

  totRecursos = totRelesLocais + totSensoresLocais + totRecursosRemotos;
  recursos = (Recurso *)calloc(sizeof(Recurso), totRecursos);
  if (!recursos) {
    // TODO :: DIE!
  }
  
  for (int r=1; r <= totRelesLocais; r++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    recurso->tipo   = RECURSO_RELE;
    recurso->num    = r;
    recurso->remoto = false;
    recurso->device = releGet(r);
  }

  for (int s=1; s <= totSensoresLocais; s++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    recurso->tipo   = RECURSO_SENSOR;
    recurso->num    = s;
    recurso->remoto = false;
    recurso->device = sensorGet(s);
  }

  for (int rr=1; rr <= totRecursosRemotos; rr++) {
    Recurso *recurso = &recursos[recursoAddCount++];

    String tipo = getPrefsAtr(prefs, rr, "tipo");
    int numNodo = atoi(getPrefsAtr(prefs, rr, "nodo").c_str());

    recurso->tipo = tipo == "1" ? RECURSO_RELE : 
      (tipo == "2" ? RECURSO_SENSOR : RECURSO_BOTAO);
    recurso->num    = getPrefsAtr(prefs, rr, "num").toInt();
    recurso->remoto = true;

    recurso->device = nodoRemotoGet(numNodo);
    recursoPrint(recurso);
  }

  prefs.end();
}

int recursosGetCount() {
  return totRecursos;
}

Recurso *recursoGet(TipoRecurso tipo, int num) {
  int tot = recursosGetCount();
  for (int r=0; r < tot; r++) {
    Recurso *recurso = &recursos[r];
    if (recurso->tipo == tipo && recurso->num == num) {
      return recurso;
    }
  }
  return NULL;
}

void recursoPrint(Recurso *recurso) {
  logaMensagem("Recurso%s %d:%s (%s) > [%s]",
      recurso->remoto ? " Remoto" : "", recurso->num,
      recurso->tipo == RECURSO_RELE ? "RELE" : (recurso->tipo == RECURSO_SENSOR ? "SENSOR" : "BOTAO"));
}
