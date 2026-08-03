#include <Arduino.h>
#include <Preferences.h>

struct Mestre
{
    String mac;
    IPAddress ip;
    bool online;
    uint32_t ultimoHeartbeat;
};

void mestreInit(Preferences &prefs);
void mestreCheckDiscover(String mac, IPAddress ip);
