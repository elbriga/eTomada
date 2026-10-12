#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define NODOS_PATH "/nodosRemotos.json"

enum TipoNodoRemoto
{
  TIPO_NODO_DESCONHECIDO = 0,
  TIPO_NODO_LITE = 13,
  TIPO_NODO_FULL = 33
};

struct NodoRemoto
{
  char id[32];
  TipoNodoRemoto tipo;
  IPAddress ip;
  char descricao[64];
  int recursosCount;
  bool refreshPendente;
  bool del;
  bool online;
  uint32_t ultimoContato;
  uint32_t ultimoTeste;
};

void nodoRemotoInit();
int nodosRemotosGetCount();

NodoRemoto *nodoRemotoGet(const char *id);

void nodosRemotosRefreshCtl(bool liga);

void nodoRemotoPrint(NodoRemoto *nodoRemoto);

void nodoRemotoCalcRecursos();

JsonDocument nodosRemotosGetJSON(NodoRemoto *novoNodo = nullptr, bool full = true);
String nodoRemotoAddFromJSON(uint8_t *json);
String nodoRemotoDelFromJSON(uint8_t *json);

void nodoRemotoEventoBoot(const char *novoID, const char *tipo, IPAddress novoIP);
int nodosRemotosGetNovosCount();
JsonDocument nodosRemotosGetNovosJSON();

const char *nodoRemotoGetTipoStr(TipoNodoRemoto tipo);
TipoNodoRemoto nodoRemotoGetTipoFromStr(String tipo);
