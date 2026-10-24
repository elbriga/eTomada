#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "rele.h"
#include "sensor.h"
#include "botao.h"
#include "umidificador.h"
#include "tipoRecurso.h"
#include "eventos.h"

struct Recurso
{
  char id[32];
  TipoRecurso tipo;
  char nome[32];
  union
  {
    Rele *rele;
    Sensor *sensor;
    Botao *botao;
    Umidificador *umid;
  };
};

void recursosInit();
int recursosGetCount();
void recursosZera();

Recurso *recursoGet(const char *id);
Recurso *recursoGetPorIndice(int posicao);

// Altera o recurso > acoes
bool recursoSetFromJSON(uint8_t *json, String &recursoIDOut, String &msgOut, bool enviaMestre = true);
bool recursoSet(const char *recursoID, String estado, String &msgOut, bool enviaMestre = true, bool enviaEventos = true);

// Atualiza o recurso > eventos
String recursoAtualizaFromJsonLocked(Recurso *recurso, JsonDocument doc, bool enviaEventos);

const char *recursoGetTipoStr(TipoRecurso tipo);
TipoRecurso recursoGetTipoFromStr(String tipoStr);
JsonDocument recursoGetJSONDoc(Recurso *r);

String recursoGetJSONEvento(const char *recursoID, TipoEvento tipoEvento);
String recursoEventoRecebido(uint8_t *json, IPAddress ip);

String recursoAtualizaConfigFromJSON(uint8_t *json);

void recursoEnviaSSE(const char *recursoID);

int recursoGetValor(Recurso *r);
void recursoPrint(Recurso *recurso);
