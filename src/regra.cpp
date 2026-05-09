#include <Arduino.h>
#include "rele.h"

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
  if (regra.length() < 10) {
    return "len";
  }

  char acao[3] = {0};
  char param1[17] = {0};
  char param2[17] = {0};
  int lidos = sscanf(regra.c_str(), "%2[^-]-%16[^-]-%16[^-]", acao, param1, param2);
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

  return "";
}

String checkRegra(int num, struct tm timeinfo) {
  Rele *rele = &reles[num];

  String regraOK = validaRegra(rele->regra);
  if (regraOK != "") {
    return "Regra[" + String(num+1) + "] Invalida:" + regraOK;
  }

  int hI=-1, mI=-1, hF=-1, mF=-1;
  char ligar[3] = {0};
  sscanf(rele->regra.c_str(), "%2[^-]-%d:%d-%d:%d", ligar, &hI, &mI, &hF, &mF);

  int tsI = hI * 60 + mI;
  int tsF = hF * 60 + mF;
  int tsAgora = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool virouDia = tsF < tsI;
  bool estaNoIntervalo = !virouDia ?
    (tsAgora >= tsI && tsAgora <= tsF) :
    (tsAgora >= tsI || tsAgora <= tsF);

  bool acaoEhLigar = !strncmp(ligar, "ON", 2);
  if (!acaoEhLigar) estaNoIntervalo = !estaNoIntervalo;

  // Serial.printf("tsI=%d\ntsF=%d\ntsAgora=%d\n", tsI, tsF, tsAgora);
  // if (estaNoIntervalo) Serial.printf("LIGAR [%s]\n", nomeReles[num]);
  // else                 Serial.printf("DESLIGAR [%s]\n", nomeReles[num]);

  return releControla(num + 1, estaNoIntervalo);
}
