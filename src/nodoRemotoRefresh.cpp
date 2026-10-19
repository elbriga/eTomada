#include <atomic>
#include <ArduinoJson.h>

#include "eTomada.h"
#include "loga.h"
#include "mutex.h"
#include "nodoRemoto.h"
#include "recursoRemoto.h"
#include "apiInterna.h"
#include "regras.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("NRREFRH", nivel, fmt, ##__VA_ARGS__)

#define NRR_TIMEOUT_OFFLINE 90 * 1000

std::atomic<bool> nodosRemotosRefreshRodando{false};
std::atomic<bool> nodosRemotosRefreshRequestOFF{false};

void nodosRemotosRefreshTask(void *args)
{
  char nodoID[32];
  IPAddress ip;
  bool fazRefresh;
  bool pula = false;

  nodosRemotosRefreshRodando = true;

  // for infinito sem o total fora do lock
  // reinicia nr=0 quando chega no final
  for (int nr = 0;; nr++)
  {
    if (nodosRemotosRefreshRequestOFF)
    {
      nodosRemotosRefreshRequestOFF = false;
      break;
    }

    // Com o delay aqui em cima os "continue" sempre respeitam o delay
    if (!pula)
      vTaskDelay(pdMS_TO_TICKS(100));
    pula = false;

    ip = IPAddress(); // zerar

    // Pegar os dados com LOCK
    {
      MutexLock lock(modeloMutex, "nodosRemotosRefreshTask1");
      if (!lock)
        continue;

      NodoRemoto *nodoRemoto = nodoRemotoGetPorIndice(nr);
      if (!nodoRemoto)
      {
        nr = -1; // FORÇAR O RESET DO FOR!!!! = LOOP INFINITO
        continue;
      }

      // Nodo sem IP? nao esta cadastrado no DNS
      if (!nodoRemoto->ip)
      {
        pula = true;
        continue;
      }

      uint32_t tempoDesdeUltimoContato = millis() - nodoRemoto->ultimoContato;
      if (tempoDesdeUltimoContato > NRR_TIMEOUT_OFFLINE)
      {
        if (nodoRemoto->online)
          logaM(LOG_AVISO, "Nodo [%s] OFFLINE!", nodoRemoto->id);
        nodoRemoto->online = false;
      }

      if (millis() - nodoRemoto->ultimoTeste < 20 * 1000)
        continue;

      if (nodoRemoto->refreshPendente ||
          !nodoRemoto->online ||
          tempoDesdeUltimoContato > 30 * 1000)
      {
        nodoRemoto->ultimoTeste = millis();

        strlcpy(nodoID, nodoRemoto->id, sizeof(nodoID));
        ip = nodoRemoto->ip;

        fazRefresh = nodoRemoto->refreshPendente;
        // if (fazRefresh)
        //   logaM(LOG_NORMAL, "Fazer refresh de [%s]", nodoID);
        // else
        //   logaM(LOG_NORMAL, "Fazer PING para [%s]", nodoID);
      }
    }

    if (!ip)
      continue;

    if (fazRefresh)
    {
      String msg;
      JsonDocument snapshot;
      // HTTP sem o LOCK
      if (!apiInternaGetSnapshot(nodoID, ip, snapshot, msg))
      {
        logaM(LOG_AVISO, "nodosRemotosRefreshTask > apiIntGetSnapshot > [%s]", msg.c_str());
        snapshot.clear();
        continue;
      }

      {
        MutexLock lock(modeloMutex, "nodosRemotosRefresh2");
        if (!lock)
          continue;

        NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
        if (!nodoRemoto)
          continue;

        recursoRemotoAtualizaFromSnapshotLocked(nodoRemoto, snapshot);
      }

      snapshot.clear();

      // regrasBoot fora do Lock
      bool regrasBootOK = regrasBoot(nodoID);

      {
        MutexLock lock(modeloMutex, "nodosRemotosRefreshTask3");
        if (!lock)
          continue;

        NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
        if (!nodoRemoto)
        {
          logaM(LOG_AVISO, "nodosRemotosRefreshTask :: nodo sumiu!");
          continue;
        }

        nodoRemoto->refreshPendente = !regrasBootOK;
        nodoRemoto->online = true;
        nodoRemoto->ultimoContato = millis();
        nodoRemoto->ultimoTeste = millis();
      }

      continue;
    }

    // Fazer o PING
    String msg;
    bool online = apiInternaPing(nodoID, ip, msg);

    {
      MutexLock lock(modeloMutex, "nodosRemotosRefreshTask4");
      if (!lock)
        continue;

      NodoRemoto *nodoRemoto = nodoRemotoGet(nodoID);
      if (!nodoRemoto)
      {
        logaM(LOG_AVISO, "nodosRemotosRefreshTask :: nodo sumiu 2!");
        continue;
      }

      nodoRemoto->ultimoTeste = millis();

      if (online)
      {
        if (!nodoRemoto->online)
        {
          logaM(LOG_AVISO, "Nodo[%s] Ficou Online!", nodoRemoto->id);
          nodoRemoto->refreshPendente = true;
          nodoRemoto->ultimoTeste = millis() - 30000UL; // para o refresh
        }

        nodoRemoto->online = true;
        nodoRemoto->ultimoContato = millis();
      }
    }
  }

  logaM(LOG_NORMAL, "Encerrando Task de Refresh");

  nodosRemotosRefreshRodando = false;
  vTaskDelete(NULL);
}

void nodosRemotosRefreshCtl(bool liga)
{
  if (!liga) // Desligar a Task
  {
    if (!nodosRemotosRefreshRodando)
      return;

    // Solicitar o desligamento
    nodosRemotosRefreshRequestOFF = true;

    // Aguardar!
    uint32_t inicio = millis();
    uint32_t t3, t3i = 0;
    while (nodosRemotosRefreshRodando)
    {
      vTaskDelay(pdMS_TO_TICKS(25));

      t3 = (millis() - inicio) / 3000;
      if (t3i != t3)
      {
        t3i = t3;
        logaM(LOG_AVISO, "AGUARDANDO encerramento da Task de refresh a [%d]s", t3 * 3);
      }
    }

    return;
  }

  if (!nodosRemotosGetCount())
  {
    logaM(LOG_AVISO, "nodosRemotosRefresh OFF > SEM NODOS REMOTOS");
    return;
  }

  if (nodosRemotosRefreshRodando)
  {
    logaM(LOG_CRITICO, "nodosRemotosRefreshCtl(ON) > JA ATIVO!!!!");
    return;
  }

  nodosRemotosRefreshRodando = true;
  nodosRemotosRefreshRequestOFF = false;

  if (xTaskCreate(
          nodosRemotosRefreshTask,
          "nrRefresh",
          8192 * 2,
          nullptr,
          1,
          NULL) != pdPASS)
  {
    logaM(LOG_CRITICO, "ERRO ao criar Task de refresh!!!");
    nodosRemotosRefreshRodando = false;
  }
}
