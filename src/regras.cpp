#include <Arduino.h>

#include "eTomada.h"
#include "regras.h"
#include "loga.h"
#include "display.h"
#include "rele.h"
#include "sensor.h"
#include "ntp.h"
#include "mutex.h"

// REQUIRE releMutex, sensorMutex locked
String checkRegra(Rele *rele)
{
  if (!rele)
  {
    return "Rele invalido!!!";
  }

  if (!strlen(rele->regra))
  {
    return "";
  }

  // Verificar se esta em modo manual
  if (rele->override > time(nullptr))
  {
    // TODO Bug 2038! kkk
    return "";
  }

  char acao[3] = {rele->regra[0], rele->regra[1], 0};

  if (!strcmp(acao, "ON") || !strcmp(acao, "OF"))
  {
    int hI = -1, mI = -1, hF = -1, mF = -1;
    char ligar[3] = {0};
    sscanf(rele->regra, "%2[^|]|%d:%d|%d:%d", ligar, &hI, &mI, &hF, &mF);

    int tsI = hI * 60 + mI;
    int tsF = hF * 60 + mF;

    struct tm timeinfo;
    ntpGetTime(&timeinfo);
    int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    bool virouDia = tsF < tsI;
    bool estaNoIntervalo = !virouDia ? (tsAgora >= tsI && tsAgora <= tsF) : (tsAgora >= tsI || tsAgora <= tsF);

    bool acaoEhLigar = !strncmp(ligar, "ON", 2);
    if (!acaoEhLigar)
      estaNoIntervalo = !estaNoIntervalo;

    return releControlaUnsafe(rele, estaNoIntervalo);
  }

  if (!strcmp(acao, "SE"))
  {
    char se[3] = {0}, condLiga[32] = {0}, condDesliga[32] = {0};
    sscanf(rele->regra, "%2[^|]|%31[^|]|%31s", se, condLiga, condDesliga);

    // Verificar a condicao LIGA:
    if (condLiga[0] == 'S')
    {
      int numSensor = condLiga[1] - '0';
      if (numSensor < 1 || numSensor > MAX_SENSORES)
      {
        return "Sensor ON invalido";
      }
      Sensor *sensor = sensorGet(numSensor);
      if (!sensor)
      {
        return "Sensor ON invalido!";
      }
      if (!sensor->ativo)
      {
        // return "Sensor ON Inativo";
        return "";
      }

      bool ligaRele;
      int valorTeste = atoi(&condLiga[3]);
      if (condLiga[2] == '>')
      {
        ligaRele = (sensor->valor > valorTeste);
      }
      else if (condLiga[2] == '<')
      {
        ligaRele = (sensor->valor < valorTeste);
      }
      else
      {
        return "Condicao ON invalida!";
      }

      if (ligaRele)
      {
        return releControlaUnsafe(rele, true);
      }
    }

    // Verificar a condicao DESLIGA:
    if (condDesliga[0] == 'S')
    {
      int numSensor = condDesliga[1] - '0';
      if (numSensor < 1 || numSensor > MAX_SENSORES)
      {
        return "Sensor OF invalido";
      }
      Sensor *sensor = sensorGet(numSensor);
      if (!sensor)
      {
        return "Sensor OF invalido!";
      }
      if (!sensor->ativo)
      {
        // return "Sensor OF Inativo";
        return "";
      }

      bool desligaRele;
      int valorTeste = atoi(&condDesliga[3]);
      if (condDesliga[2] == '>')
      {
        desligaRele = (sensor->valor > valorTeste);
      }
      else if (condDesliga[2] == '<')
      {
        desligaRele = (sensor->valor < valorTeste);
      }
      else
      {
        return "Condicao OF invalida!";
      }

      if (desligaRele)
      {
        return releControlaUnsafe(rele, false);
      }
    }

    return "";
  }

  return "Acao invalida";
}

void processaRegras()
{
  if (eTomadaGetModoOperacao() != MODO_CONTROLADOR)
  {
    return;
  }

  String msgDisplay = "";
  {
    MutexLock lockRecursos(recursosMutex);

    if (!lockRecursos)
    {
      logaMensagem("processaRegras: erro mutex");
      return;
    }

    int totReles = relesGetCount();
    for (int r = 1; r <= totReles; r++)
    {
      Rele *rele = releGet(r);
      if (!rele->ativo)
        continue;

      String msg = checkRegra(rele);
      if (msg != "")
      {
        logaMensagem(msg.c_str());
        msgDisplay = msg; // Mostra no display a ultima msg
      }
    }
  }

  if (msgDisplay != "")
  {
    displayMostraMsg(msgDisplay.c_str(), 5000, false);
  }
}

String validaHora(String hora)
{
  int h, m;
  int lidos = sscanf(hora.c_str(), "%d:%d", &h, &m);
  if (lidos != 2)
  {
    return "campos";
  }
  if (h < 0 || h > 23)
  {
    return "hora";
  }
  if (m < 0 || m > 59)
  {
    return "minuto";
  }
  return "";
}

String validaCondicao(const char *condicao)
{
  if (condicao[0] != 'S')
  {
    return "Sensor Invalido";
  }
  int numSensor = condicao[1] - '0';
  if (numSensor < 1 || numSensor > sensoresGetCount())
  {
    return "Sensor Invalido!";
  }

  if (condicao[2] != '<' && condicao[2] != '>')
  {
    return "Op Invalida";
  }

  if (!strlen(&condicao[3]))
  {
    return "valTeste vazio";
  }
  // TODO validar valTeste
  // int valTeste = atoi(&condicao[3]);

  return "OK";
}

String validaRegra(String regra)
{
  if (regra == "")
  {
    return "OK";
  }

  if (regra.length() < 10)
  {
    return "len";
  }

  char acao[3] = {0};
  char param1[33] = {0};
  char param2[33] = {0};
  int lidos = sscanf(regra.c_str(), "%2[^|]|%32[^|]|%32[^|]", acao, param1, param2);
  if (lidos < 3)
  {
    return "campos:" + String(lidos) + ":" + String(acao) + ":" + String(param1) + ":" + String(param2);
  }

  // Validar as Acoes
  if (!strncmp(acao, "ON", 2) || !strncmp(acao, "OF", 2))
  {
    String hOK = validaHora(param1);
    if (hOK != "")
    {
      return "hI:" + hOK;
    }
    hOK = validaHora(param2);
    if (hOK != "")
    {
      return "hF:" + hOK;
    }
  }
  else if (!strncmp(acao, "SE", 2))
  {
    String condOK = validaCondicao(param1);
    if (condOK != "OK")
    {
      return "condON:" + condOK;
    }
    condOK = validaCondicao(param2);
    if (condOK != "OK")
    {
      return "condOF:" + condOK;
    }
  }
  else
  {
    return "acao";
  }

  return "OK";
}
