#include <Arduino.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "discover.h"

NodoRemoto *nodosRemotos = 0;
static int totNodosRemotos = 0;

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

    // Escanear
    discoverStart();
    long start = millis();
    while (millis() - start < 5000) {
      if (!getDiscoverTaskRunning())
        break;
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (getDiscoverTaskRunning()) {
      // TODO :: Erro!!
    }
    
    for (int nr=1; nr <= totNodosRemotos; nr++) {
      NodoRemoto *nodoRemoto = &nodosRemotos[nr - 1];

      nodoRemoto->num = nr;

      strncpy(nodoRemoto->deviceID, getPrefsAtr(prefs, nr, "deviceID").c_str(),  sizeof(nodoRemoto->deviceID) - 1);
      nodoRemoto->deviceID[sizeof(nodoRemoto->deviceID) - 1] = '\0';

      NodoRemoto *nodoDescoberto = getDiscoverNodo(nodoRemoto->deviceID);
      if (nodoDescoberto) {
        nodoRemoto->ip = nodoDescoberto->ip;
      } else {
        nodoRemoto->ip = (uint32_t)0;
        logaMensagem("Nodo Remoto %d OFFLINE", nr);
      }
      
      nodoRemotoPrint(nodoRemoto);
    }
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

void nodoRemotoPrint(NodoRemoto *nodoRemoto) {
  logaMensagem("NodoRemoto %d > %s > [%s]",
      nodoRemoto->num, nodoRemoto->deviceID,
      nodoRemoto->ip.toString().c_str());
}
