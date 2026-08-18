#pragma once
#include <ArduinoJson.h>
#include "nodoRemoto.h"

void discoverInit();
void discoverLoopNo();

void discoverStart(bool ehTask);
bool discoverWaitRun(bool ehTask);

bool discoverGetTaskRunning();
int discoverGetNodosCount();

NodoRemoto *discoverGetNodo(const char *deviceID);
NodoRemoto *discoverGetNodoPorIndice(int i);
JsonDocument *discoverGetNodoSnapshot(const char *deviceID);
