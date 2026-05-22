#pragma once
#include <Preferences.h>

// Salvar as configuracoes na memoria FLASH
String getPrefsAtr(Preferences &prefs, int num, String nomeAtr);
String setPrefsAtr(Preferences &prefs, int num, String nomeAtr, String val);
