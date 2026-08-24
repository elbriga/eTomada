#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "loga.h"

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("UTIL", nivel, fmt, ##__VA_ARGS__)

void utilDIE(const char *msg)
{
  logaM(LOG_CRITICO, ">>> DIE!!! [%s]", msg);
  ESP.restart();
}

void utilRestart(const char *msg)
{
  logaM(LOG_AVISO, ">>> Restart!!! [%s]", msg);

  // TODO :: Salvar estado dos reles

  delay(100);
  ESP.restart();
}

int utilVersionToInt(const char *ver)
{
  int v1, v2, v3;

  sscanf(ver, "%d.%d.%d", &v1, &v2, &v3);

  return v1 * 1000000 + v2 * 1000 + v3;
}

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino)
{
  File origem = LittleFS.open(pathOrigem, "r");
  if (!origem)
  {
    logaM(LOG_AVISO, "ERRO: utilCopiaArquivo [%s] nao encontrado", pathOrigem);
    return 1;
  }

  File destino = LittleFS.open(pathDestino, "w");

  if (!destino)
  {
    origem.close();
    logaM(LOG_CRITICO, "ERRO: utilCopiaArquivo impossivel criar [%s]", pathDestino);
    return 2;
  }

  int bytesCopiados = 0;
  uint8_t buffer[256];
  while (origem.available())
  {
    size_t lidos = origem.read(buffer, sizeof(buffer));
    if (lidos > 0)
    {
      size_t gravados = destino.write(buffer, lidos);
      bytesCopiados += gravados;
    }
  }

  origem.close();
  destino.close();

  logaM(LOG_NORMAL, "Arquivo [%s] copiado para [%s] - %d bytes", pathOrigem, pathDestino, bytesCopiados);

  return 0;
}

const char *utilGetDiaSemana(struct tm timeinfo)
{
  switch (timeinfo.tm_wday)
  {
  case 0:
    return "Dom";
  case 1:
    return "Seg";
  case 2:
    return "Ter";
  case 3:
    return "Qua";
  case 4:
    return "Qui";
  case 5:
    return "Sex";
  case 6:
    return "Sab";
  default:
    return "---";
  }
}

// TODO :: Usar!
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, uint8_t *str)
{
  DeserializationError erro = deserializeJson(doc, str);

  if (erro)
    logaM(LOG_AVISO, ">>> ERRO JSON STR* em [%s] : [%s]", onde, erro.c_str());

  return erro;
}
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, String jsonStr)
{
  DeserializationError erro = deserializeJson(doc, jsonStr);

  if (erro)
    logaM(LOG_AVISO, ">>> ERRO JSON STR em [%s] : [%s]", onde, erro.c_str());

  return erro;
}
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, File file)
{
  DeserializationError erro = deserializeJson(doc, file);

  if (erro)
    logaM(LOG_AVISO, ">>> ERRO JSON FILE em [%s] : [%s]", onde, erro.c_str());

  return erro;
}
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, WiFiUDP udp)
{
  DeserializationError erro = deserializeJson(doc, udp);

  if (erro)
    logaM(LOG_AVISO, ">>> ERRO JSON UDP em [%s] : [%s]", onde, erro.c_str());

  return erro;
}
DeserializationError utilLeJson(const char *onde, JsonDocument &doc, WiFiClient &wifiClient)
{
  DeserializationError erro = deserializeJson(doc, wifiClient);

  if (erro)
    logaM(LOG_AVISO, ">>> ERRO JSON WIFICLI em [%s] : [%s]", onde, erro.c_str());

  return erro;
}
