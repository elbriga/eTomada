#include "nodoRemoto.h"

void discoverInit();
void discoverLoop();

void discoverStart();
bool getDiscoverTaskRunning();
int getDiscoverNodosCount();
NodoRemoto *getDiscoverNodo(const char *deviceID);
