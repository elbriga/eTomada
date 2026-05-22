#include "prefs.h"

String getPrefsAtr(Preferences &prefs, int num, String nomeAtr) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%s%d", nomeAtr.c_str(), num);
  return prefs.isKey(buff) ? prefs.getString(buff, "") : "";
}

String setPrefsAtr(Preferences &prefs, int num, String nomeAtr, String val) {
  String old = getPrefsAtr(prefs, num, nomeAtr);

  if (val != old) {
    char buff[32];
    snprintf(buff, sizeof(buff), "%s%d", nomeAtr.c_str(), num);
    prefs.putString(buff, val);
  }

  return old;
}
