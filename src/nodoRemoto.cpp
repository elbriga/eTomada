#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "discover.h"

NodoRemoto *nodosRemotos = 0;
static int totNodosRemotos = 0;

void nodosRemotosRefreshTask(void *args);

void nodoRemotoInit() {
  Preferences prefs;
  prefs.begin("nodosRemotos", false);

  // Para testes
  // prefs.putString("total0", "1");
  // prefs.putString("deviceID1", "20:07:69:75:06:DC");
  
  totNodosRemotos = getPrefsAtr(prefs, 0, "total").toInt();
  logaMensagem("Nodos Remotos: %d", totNodosRemotos);

  if (totNodosRemotos > 0) {
    nodosRemotos = (NodoRemoto *)calloc(sizeof(NodoRemoto), totNodosRemotos);
    if (!nodosRemotos) {
      // TODO :: DIE!
    }

    // Inicializar
    for (int nr=1; nr <= totNodosRemotos; nr++) {
      NodoRemoto *nodoRemoto = &nodosRemotos[nr - 1];

      nodoRemoto->num = nr;

      strncpy(nodoRemoto->deviceID, getPrefsAtr(prefs, nr, "deviceID").c_str(),  sizeof(nodoRemoto->deviceID) - 1);
      nodoRemoto->deviceID[sizeof(nodoRemoto->deviceID) - 1] = '\0';

      nodoRemotoPrint(nodoRemoto);
    }

    nodosRemotosRefreshTask(nullptr);
  }

  prefs.end();
}

int nodosRemotosGetCount() {
  return totNodosRemotos;
}

NodoRemoto *nodoRemotoGet(int num) {
  if (num > 0 && num <= nodosRemotosGetCount()) {
    return &nodosRemotos[num - 1];
  }
  return NULL;
}

void nodosRemotosRefresh() {
  const char *argsFlagTask = "TASK";
  xTaskCreate(
        nodosRemotosRefreshTask,
        "nrRefresh",
        4096,
        (void *)argsFlagTask,
        1,
        NULL
    );
}

void nodosRemotosRefreshTask(void *args) {
  bool ehTask = args && !strncmp((char *)args, "TASK", 4);

  // Escanear
  discoverWaitRun(ehTask);

  int totNR = nodosRemotosGetCount();
  for (int nr=1; nr <= totNR; nr++) {
    NodoRemoto *nodoRemoto = nodoRemotoGet(nr);

    // Buscar este deviceID nos nodos escaneados
    NodoRemoto *nodoDescoberto = getDiscoverNodo(nodoRemoto->deviceID);
    if (nodoDescoberto) {
      if (nodoRemoto->ip != nodoDescoberto->ip) {
        nodoRemoto->ip = nodoDescoberto->ip;
        logaMensagem("Nodo Remoto %d - Novo IP: %s",
          nr, nodoRemoto->ip.toString().c_str());
      }
    } else {
      nodoRemoto->ip = (uint32_t)0;
      logaMensagem("Nodo Remoto %d OFFLINE", nr);
    }
  }

  if (ehTask) {
    vTaskDelete(NULL);
  }
}

void nodoRemotoPrint(NodoRemoto *nodoRemoto) {
  logaMensagem("NodoRemoto %d > %s > [%s]",
      nodoRemoto->num, nodoRemoto->deviceID,
      nodoRemoto->ip.toString().c_str());
}
