#pragma once

#include "Arduino.h"
#include "GxDEPG0150BN/GxDEPG0150BN.h" // 1.54" b/w 200x200
#include "GxEPD.h"
#include "os_config.h"

#include "lib/ui.h"

#include "Preferences.h"
#include "resources/fonts/Outfit_60011pt7b.h"
#include "resources/fonts/Outfit_80036pt7b.h"
#include "resources/icons.h"

int calculateBatteryStatus();
float readBatteryVoltage();
void setPreviousVoltage(Preferences *preferences);
bool isBatteryCharging(GxEPD_Class *display, Preferences *preferences);