#include <Arduino.h>
#include <LittleFS.h>

#include "loga.h"

void utilDIE(const char *msg)
{
  logaMensagem(">>> DIE!!! [%s]", msg);
  ESP.restart();
}

int utilCopiaArquivo(const char *pathOrigem, const char *pathDestino)
{
  File origem = LittleFS.open(pathOrigem, "r");
  if (!origem)
  {
    logaMensagem("ERRO: utilCopiaArquivo [%s] nao encontrado", pathOrigem);
    return 1;
  }

  File destino = LittleFS.open("/automacoes.json", "w");

  if (!destino)
  {
    origem.close();
    logaMensagem("ERRO: utilCopiaArquivo impossivel criar [%s]", pathDestino);
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

  logaMensagem("Arquivo [%s] copiado para [%s] - %d bytes", pathOrigem, pathDestino, bytesCopiados);

  return 0;
}
