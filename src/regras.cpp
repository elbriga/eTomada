#include <Arduino.h>

#include "regras.h"
#include "loga.h"
#include "display.h"
#include "reles.h"
#include "ntp.h"
#include "mutex.h"

void processaRegras() {
  if (xSemaphoreTake(releMutex, pdMS_TO_TICKS(1000))) {
    Rele *rele;
    int totReles = relesGetCount();
    for (int r=1; r <= totReles; r++) {
      rele = releGet(r);
      if (!rele->ativo) continue;

      String msg = checkRegra(r);
      if (msg != "") {
        logaMensagem(msg.c_str());
        displayMostraMsg(msg.c_str(), 5000);
      }
    }

    xSemaphoreGive(releMutex);
  }
}

String validaHora(String hora) {
  int h, m;
  int lidos = sscanf(hora.c_str(), "%d:%d", &h, &m);
  if (lidos != 2) {
    return "campos";
  }
  if (h < 0 || h > 23) {
    return "hora";
  }
  if (m < 0 || m > 59) {
    return "minuto";
  }
  return "";
}

String validaRegra(String regra) {
  if (regra == "") {
    return "OK";
  }

  if (regra.length() < 10) {
    return "len";
  }

  char acao[3] = {0};
  char param1[17] = {0};
  char param2[17] = {0};
  int lidos = sscanf(regra.c_str(), "%2[^>]>%16[^-]-%16[^-]", acao, param1, param2);
  if (lidos < 3) {
    return "campos:" + String(lidos) + ":" + String(acao) + ":" + String(param1) + ":" + String(param2);
  }

  // Validar as Acoes
  if (!strncmp(acao, "ON", 2) || !strncmp(acao, "OF", 2)) {
    String hOK = validaHora(param1);
    if (hOK != "") {
      return "hI:" + hOK;
    }
    hOK = validaHora(param2);
    if (hOK != "") {
      return "hF:" + hOK;
    }
  } else {
    return "acao";
  }

  return "OK";
}

String checkRegra(int numRele) {
  Rele *rele = releGet(numRele);
  if (!rele) {
    return "Rele invalido";
  }

  if (!strlen(rele->regra)) {
    return "";
  }

  String regraOK = validaRegra(rele->regra);
  if (regraOK != "OK") {
    return "Regra[" + String(numRele) + "] Invalida:" + regraOK;
  }

  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligar[3] = {0};
  sscanf(rele->regra, "%2[^>]>%d:%d-%d:%d", ligar, &hI, &mI, &hF, &mF);

  int tsI = hI * 60 + mI;
  int tsF = hF * 60 + mF;

  struct tm timeinfo;
  ntpGetTime(&timeinfo);
  int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  bool virouDia = tsF < tsI;
  bool estaNoIntervalo = !virouDia ?
    (tsAgora >= tsI && tsAgora <= tsF) :
    (tsAgora >= tsI || tsAgora <= tsF);

  bool acaoEhLigar = !strncmp(ligar, "ON", 2);
  if (!acaoEhLigar) estaNoIntervalo = !estaNoIntervalo;

  return releControla(numRele, estaNoIntervalo);
}
