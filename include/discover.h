#include "nodoRemoto.h"

void discoverInit();
void discoverLoop();

void discoverStart(bool ehTask);
void discoverWaitRun(bool ehTask);

bool getDiscoverTaskRunning();
int getDiscoverNodosCount();
NodoRemoto *getDiscoverNodo(const char *deviceID);
