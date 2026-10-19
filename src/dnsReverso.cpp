#include <mutex>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>

#define DNS_REVERSO_CACHE_SIZE 16
#define DNS_REVERSO_CACHE_TTL 24 * 60 * 60 * 1000UL // 24 horas
#define DNS_REVERSO_CACHE_ERRO_TTL 10 * 60 * 1000UL // 10 minutos

struct DnsReversoCacheEntry
{
    IPAddress ip;
    String nome;
    uint32_t timestamp = 0;
    bool usado = false;
};

static DnsReversoCacheEntry dnsReversoCache[DNS_REVERSO_CACHE_SIZE];
static uint8_t dnsReversoCacheProximo = 0;
static std::mutex dnsReversoCacheMutex;

String dnsReversoGet(IPAddress ip, uint32_t timeoutMs = 500);

String dnsReverso(IPAddress ip, uint32_t timeoutMs)
{
    // Consulta apenas para IPs da rede 10
    if (ip[0] != 10)
        return ip.toString();

    uint32_t agora = millis();

    // Procura no cache
    {
        std::lock_guard<std::mutex> lock(dnsReversoCacheMutex);

        for (auto &e : dnsReversoCache)
        {
            if (!e.usado || e.ip != ip)
                continue;

            uint32_t ttl = e.nome.isEmpty()
                               ? DNS_REVERSO_CACHE_ERRO_TTL
                               : DNS_REVERSO_CACHE_TTL;

            if (agora - e.timestamp < ttl)
                return e.nome;

            break;
        }
    }

    // Cache miss: consulta DNS sem segurar o mutex
    String nome = dnsReversoGet(ip, timeoutMs);
    agora = millis();

    // Atualiza cache
    {
        std::lock_guard<std::mutex> lock(dnsReversoCacheMutex);

        int indice = -1;

        // Prioriza atualizar entrada existente ou reutilizar vaga
        for (int i = 0; i < DNS_REVERSO_CACHE_SIZE; i++)
        {
            auto &e = dnsReversoCache[i];

            if (e.usado && e.ip == ip)
            {
                indice = i;
                break;
            }

            if (indice < 0 && !e.usado)
                indice = i;
        }

        // Cache cheio: substituição circular
        if (indice < 0)
        {
            indice = dnsReversoCacheProximo;
            dnsReversoCacheProximo =
                (dnsReversoCacheProximo + 1) % DNS_REVERSO_CACHE_SIZE;
        }

        auto &e = dnsReversoCache[indice];

        e.ip = ip;
        e.nome = nome;
        e.timestamp = agora;
        e.usado = true;
    }

    return nome;
}

static uint16_t dnsLer16(const uint8_t *p)
{
    return (uint16_t(p[0]) << 8) | p[1];
}

// Lê nomes DNS, incluindo ponteiros de compressão
static bool dnsLerNome(const uint8_t *buf, size_t tam, size_t &pos, String &nome)
{
    size_t p = pos;
    bool comprimido = false;
    nome = "";

    for (int i = 0; i < 128; i++)
    {
        if (p >= tam)
            return false;

        uint8_t n = buf[p++];

        if (n == 0)
        {
            if (!comprimido)
                pos = p;
            return true;
        }

        if ((n & 0xC0) == 0xC0)
        {
            if (p >= tam)
                return false;

            size_t destino = ((n & 0x3F) << 8) | buf[p++];

            if (destino >= tam)
                return false;

            if (!comprimido)
                pos = p;

            comprimido = true;
            p = destino;
            continue;
        }

        if ((n & 0xC0) || p + n > tam || nome.length() + n + 1 > 253)
            return false;

        if (nome.length())
            nome += ".";

        for (int j = 0; j < n; j++)
            nome += (char)buf[p++];

        if (!comprimido)
            pos = p;
    }

    return false;
}

String dnsReversoGet(IPAddress ip, uint32_t timeoutMs)
{
    IPAddress dns = WiFi.dnsIP();

    if (WiFi.status() != WL_CONNECTED || dns == IPAddress(0, 0, 0, 0))
        return "";

    // Monta o nome reverso
    char dominio[48];
    snprintf(dominio, sizeof(dominio), "%u.%u.%u.%u.in-addr.arpa",
             ip[3], ip[2], ip[1], ip[0]);

    // Monta o pacote DNS
    uint8_t pacote[96] = {};
    uint16_t id = (uint16_t)esp_random();

    pacote[0] = id >> 8;
    pacote[1] = id & 0xFF;
    pacote[2] = 0x01; // Recursion Desired
    pacote[5] = 0x01; // Uma pergunta

    size_t tam = 12;
    const char *p = dominio;

    while (*p)
    {
        const char *ponto = strchr(p, '.');
        size_t n = ponto ? ponto - p : strlen(p);

        if (!n || n > 63 || tam + n + 6 > sizeof(pacote))
            return "";

        pacote[tam++] = n;
        memcpy(pacote + tam, p, n);
        tam += n;

        if (!ponto)
            break;

        p = ponto + 1;
    }

    pacote[tam++] = 0; // Fim do nome
    pacote[tam++] = 0;
    pacote[tam++] = 12; // QTYPE = PTR
    pacote[tam++] = 0;
    pacote[tam++] = 1; // QCLASS = IN

    WiFiUDP udp;

    if (!udp.begin(0))
        return "";

    if (!udp.beginPacket(dns, 53) ||
        udp.write(pacote, tam) != tam ||
        !udp.endPacket())
    {
        udp.stop();
        return "";
    }

    String resultado;
    uint32_t inicio = millis();

    while (millis() - inicio < timeoutMs)
    {
        int tamanho = udp.parsePacket();

        if (tamanho <= 0)
        {
            delay(1);
            continue;
        }

        IPAddress origem = udp.remoteIP();
        uint16_t porta = udp.remotePort();

        uint8_t resposta[512];

        if (tamanho > (int)sizeof(resposta))
        {
            while (udp.available())
                udp.read();
            continue;
        }

        int recebido = udp.read(resposta, tamanho);

        if (origem != dns || porta != 53 || recebido < 12 ||
            dnsLer16(resposta) != id)
            continue;

        // Resposta DNS válida, sem erro ou truncamento
        if ((resposta[2] & 0xFA) != 0x80 ||
            (resposta[3] & 0x0F) != 0 ||
            dnsLer16(resposta + 4) != 1)
            break;

        size_t pos = 12;
        String pergunta;

        if (!dnsLerNome(resposta, recebido, pos, pergunta) ||
            !pergunta.equalsIgnoreCase(dominio) ||
            pos + 4 > (size_t)recebido ||
            dnsLer16(resposta + pos) != 12 ||
            dnsLer16(resposta + pos + 2) != 1)
            break;

        pos += 4;

        uint16_t respostas = dnsLer16(resposta + 6);

        for (uint16_t i = 0; i < respostas; i++)
        {
            String nome;

            if (!dnsLerNome(resposta, recebido, pos, nome) ||
                pos + 10 > (size_t)recebido)
                break;

            uint16_t tipo = dnsLer16(resposta + pos);
            uint16_t classe = dnsLer16(resposta + pos + 2);
            uint16_t rdlen = dnsLer16(resposta + pos + 8);

            pos += 10;

            if (rdlen > (size_t)recebido - pos)
                break;

            if (tipo == 12 && classe == 1)
            {
                size_t pNome = pos;

                if (dnsLerNome(resposta, recebido, pNome, nome) &&
                    pNome == pos + rdlen)
                    resultado = nome;

                break;
            }

            pos += rdlen;
        }

        break;
    }

    udp.stop();
    return resultado;
}
