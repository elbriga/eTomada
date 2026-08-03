#include <ArduinoJson.h>
#include "nodoRemoto.h"

void discoverInit();
void discoverLoop();

void discoverStart(bool ehTask);
void discoverWaitRun(bool ehTask);

bool discoverGetTaskRunning();
int discoverGetNodosCount();

NodoRemoto *discoverGetNodo(const char *deviceID);
NodoRemoto *discoverGetNodoPorIndice(int i);
JsonDocument *discoverGetNodoSnapshot(const char *deviceID);
