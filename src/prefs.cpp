#include "prefs.h"
#include "loga.h"

String getPrefsAtr(Preferences &prefs, const char *id, const char *nomeAtr)
{
  char buff[32];
  snprintf(buff, sizeof(buff), "%s%s", nomeAtr, id);
  return prefs.isKey(buff) ? prefs.getString(buff, "") : "";
}

String setPrefsAtr(Preferences &prefs, const char *id, const char *nomeAtr, String val)
{
  String old = getPrefsAtr(prefs, id, nomeAtr);

  if (val != old)
  {
    char buff[32];
    snprintf(buff, sizeof(buff), "%s%s", nomeAtr, id);
    prefs.putString(buff, val);
  }

  return old;
}
