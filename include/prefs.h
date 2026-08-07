#pragma once
#include <Preferences.h>

// Salvar as configuracoes na memoria FLASH
String getPrefsAtr(Preferences &prefs, const char *id, const char *nomeAtr);
String setPrefsAtr(Preferences &prefs, const char *id, const char *nomeAtr, String val);
