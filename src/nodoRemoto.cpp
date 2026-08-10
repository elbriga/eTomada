#include <Arduino.h>
#include <ArduinoJson.h>

#include "eTomada.h"
#include "loga.h"
#include "prefs.h"
#include "nodoRemoto.h"
#include "discover.h"
#include "recursoRemoto.h"
#include "util.h"

NodoRemoto *nodosRemotos = 0;
static int totNodosRemotos = 0;

void nodosRemotosRefreshTask(void *args);

void nodoRemotoInit()
{
  Preferences prefs;
  prefs.begin("nodosRemotos", false);

  // Para testes
  // prefs.putString("total", "2");
  // prefs.putString("deviceID1", "20:07:69:75:06:DC");
  // prefs.putString("deviceID2", "CC:AE:54:DA:F3:80");

  totNodosRemotos = getPrefsAtr(prefs, "", "total").toInt();
  logaMensagem("Nodos Remotos: %d", totNodosRemotos);

  if (totNodosRemotos > 0)
  {
    // nodosRemotos = (NodoRemoto *)calloc(sizeof(NodoRemoto), totNodosRemotos);
    nodosRemotos = new NodoRemoto[totNodosRemotos]();
    if (!nodosRemotos)
      utilDIE("NODOS REMOTOS");

    // Inicializar
    for (int nr = 1; nr <= totNodosRemotos; nr++)
    {
      NodoRemoto *nodoRemoto = &nodosRemotos[nr - 1];

      nodoRemoto->num = nr;
      nodoRemoto->online = false;

      char num[8];
      snprintf(num, sizeof(num), "%d", nr);
      // TODO :: renomear deviceID para mac
      strncpy(nodoRemoto->deviceID, getPrefsAtr(prefs, num, "deviceID").c_str(), sizeof(nodoRemoto->deviceID) - 1);
      nodoRemoto->deviceID[sizeof(nodoRemoto->deviceID) - 1] = '\0';
    }

    nodosRemotosRefreshTask(nullptr);

    for (int nr = 1; nr <= totNodosRemotos; nr++)
    {
      NodoRemoto *nodoRemoto = &nodosRemotos[nr - 1];
      nodoRemotoPrint(nodoRemoto);
    }
  }

  prefs.end();
}

int nodosRemotosGetCount()
{
  return totNodosRemotos;
}

NodoRemoto *nodoRemotoGet(int num)
{
  if (num > 0 && num <= nodosRemotosGetCount())
  {
    return &nodosRemotos[num - 1];
  }
  return NULL;
}

NodoRemoto *nodoRemotoGetPorMAC(const char *mac)
{
  int tot = nodosRemotosGetCount();
  for (int nr = 1; nr <= tot; nr++)
  {
    NodoRemoto *nodo = nodoRemotoGet(nr);
    if (!strcmp(nodo->deviceID, mac))
    {
      return &nodosRemotos[nr - 1];
    }
  }
  return NULL;
}

void nodosRemotosRefresh()
{
  const char *argsFlagTask = "TASK";
  xTaskCreate(
      nodosRemotosRefreshTask,
      "nrRefresh",
      4096,
      (void *)argsFlagTask,
      1,
      NULL);
}

void nodosRemotosRefreshTask(void *args)
{
  bool ehTask = args && !strncmp((char *)args, "TASK", 4);

  // Escanear
  discoverWaitRun(ehTask);

  int totNR = nodosRemotosGetCount();
  int totND = discoverGetNodosCount();

  // Verificar por novos nodos
  for (int nd = 1; nd <= totND; nd++)
  {
    NodoRemoto *nodoDiscover = discoverGetNodoPorIndice(nd - 1);
    bool achei = false;
    for (int nr = 1; nr <= totNR; nr++)
    {
      NodoRemoto *nodoRemoto = nodoRemotoGet(nr);
      if (!strcmp(nodoDiscover->deviceID, nodoRemoto->deviceID))
      {
        achei = true;
        break;
      }
    }
    if (!achei)
    {
      logaMensagem(">>> Nodo [%s] encontrado em %s. Avisar na interface",
                   nodoDiscover->deviceID, nodoDiscover->ip.toString().c_str());
      // TODO
    }
  }

  for (int nr = 1; nr <= totNR; nr++)
  {
    NodoRemoto *nodoRemoto = nodoRemotoGet(nr);

    // Buscar este deviceID nos nodos escaneados
    NodoRemoto *nodoDescoberto = discoverGetNodo(nodoRemoto->deviceID);
    if (nodoDescoberto)
    {
      if (!nodoRemoto->online)
        logaMensagem("Nodo Remoto %d - ONLINE", nr);
      nodoRemoto->online = true;

      if (nodoRemoto->ip != nodoDescoberto->ip)
      {
        nodoRemoto->ip = nodoDescoberto->ip;
        logaMensagem("Nodo Remoto %d - Novo IP: %s",
                     nr, nodoRemoto->ip.toString().c_str());
      }

      nodoRemoto->ping = nodoDescoberto->ping;

      // Atualizar os RecursoRemoto com o snapshot do discover
      JsonDocument *snapshot = discoverGetNodoSnapshot(nodoRemoto->deviceID);

      recursoRemotoAtualizaFromSnapshot(nodoRemoto, snapshot);
    }
    else
    {
      nodoRemoto->ip = (uint32_t)0;
      if (nodoRemoto->online)
        logaMensagem("Nodo Remoto %d OFFLINE", nr);
      nodoRemoto->online = false;
    }
  }

  if (ehTask)
  {
    vTaskDelete(NULL);
  }
}

void nodoRemotoPrint(NodoRemoto *nodoRemoto)
{
  logaMensagem("NodoRemoto %d > %s > [%s] (%d ms)",
               nodoRemoto->num, nodoRemoto->deviceID,
               nodoRemoto->ip.toString().c_str(),
               nodoRemoto->ping);
}
