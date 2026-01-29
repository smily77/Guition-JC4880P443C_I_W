#pragma once

#include <Preferences.h>
#include <WiFi.h>
#include "lvgl.h"

namespace WifiKeyboard
{
void begin(Preferences *prefs);
void start();
const char *getSsid();
const char *getPassword();
}
