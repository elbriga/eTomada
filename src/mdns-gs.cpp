#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "eTomada.h"
#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("MDNS", nivel, fmt, ##__VA_ARGS__)

void mdnsInit(bool logar)
{
    String hostname = eTomadaDeviceID();

    MDNS.end();
    vTaskDelay(pdTICKS_TO_MS(50));

    if (MDNS.begin(hostname.c_str()))
    {
        if (logar)
            logaM(LOG_NORMAL, "mDNS iniciado em [%s]", hostname.c_str());

        if (!MDNS.addService("etomada", "tcp", 80))
        {
            logaM(LOG_CRITICO, "Erro ao add servico mDNS [%s] _etomada._tcp", hostname.c_str());
            return;
        }

        MDNS.addServiceTxt("etomada", "tcp", "device", "eTomada");
        MDNS.addServiceTxt("etomada", "tcp", "id", hostname.c_str());
        MDNS.addServiceTxt("etomada", "tcp", "ssid", WiFi.SSID().c_str());
        MDNS.addServiceTxt("etomada", "tcp", "ip", WiFi.localIP().toString().c_str());
        MDNS.addServiceTxt("etomada", "tcp", "model", eTomadaDeviceModel().c_str());
        MDNS.addServiceTxt("etomada", "tcp", "board", eTomadaDeviceBoard().c_str());
        MDNS.addServiceTxt("etomada", "tcp", "fw", eTomadaGetVersao().c_str());
        MDNS.addServiceTxt("etomada", "tcp", "api", "Full");

        if (logar)
            logaM(LOG_NORMAL, "Servico _etomada._tcp adicionado na porta 80");
    }
    else
        logaM(LOG_CRITICO, "Erro ao iniciar mDNS [%s]", hostname.c_str());
}
