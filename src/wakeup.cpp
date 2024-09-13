#include "wakeup.h"
#include "lib/battery.h"

time_t getRtcLocalTime();

// Setup

void wakeupInit(WakeupFlag *wakeupType, unsigned int *wakeupCount, GxEPD_Class *display, ESP32Time *rtc, Preferences *preferences) {
  log(LogLevel::INFO, "WAKEUP_INIT");

  preferences->putFloat("prev_volt", readBatteryVoltage());
  preferences->putBool("charging", false);

  display->update();
  display->fillScreen(GxEPD_WHITE);
  display->setTextColor(GxEPD_BLACK);
  display->setFont(&Outfit_60011pt7b);
  printCenterString(display, "Setting up!", 145, 90);
  printCenterString(display, "Please wait...", 137, 115);
  display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT);

  String ssid = preferences->getString("wifi_ssid");
  String passwd = preferences->getString("wifi_passwd");
  if (ssid == "" || passwd == "") {
    log(LogLevel::ERROR, "There are no WiFi credentials. Checking if there are credentials saved in the os_config file");
    if (WIFI_SSID == "" || WIFI_PASSWD == "") {
      log(LogLevel::ERROR, "There are no WiFi credentials. Setting RTC to last saved time value");
      setDefaultTime(rtc, preferences);
      return printWelcomeScreen(display);
    }
    log(LogLevel::SUCCESS, "Found WiFi credentials, saving them in the preferences");
    ssid = WIFI_SSID;
    passwd = WIFI_PASSWD;
    preferences->putString("wifi_ssid", ssid);
    preferences->putString("wifi_passwd", passwd);
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, passwd);
  log(LogLevel::INFO, "Checking for connection");
  int maxRetries = WIFI_RETRIES;
  for (int i = 0; WiFi.status() != WL_CONNECTED && i <= WIFI_RETRIES; i++) {
    Serial.printf(".");
    delay(1000);
  }
  Serial.printf("\n");
  if (WiFi.status() != WL_CONNECTED) {
    log(LogLevel::ERROR, "WiFi connection timed out. Setting RTC to last saved time value");
    setDefaultTime(rtc, preferences);
    return printWelcomeScreen(display);
  }
  log(LogLevel::SUCCESS, "WiFi is connected");

  getRtcLocalTime(); // we can ignore the return but use the call to update the RTC

  printWelcomeScreen(display);
}

void printWelcomeScreen(GxDEPG0150BN *display) {
  display->fillScreen(GxEPD_WHITE);
  printLeftString(display, "Welcome", 34, 90);
  printLeftString(display, "to qpaperOS", 34, 115);
  display->update();
}
void setDefaultTime(ESP32Time *rtc, Preferences *preferences) {
  time_t time = preferences->getLong64("prev_time_unix");
  if (time != 0)
    rtc->setTime(preferences->getLong64("prev_time_unix"));
  else {
    struct timeval tv;
    tv.tv_sec = 1704115200; // seconds since epoch
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) == 0) {
      Serial.println("Time set successfully!");
      delay(1000);
    } else {
      Serial.println("Failed to set time!");
    }
  }
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, nullptr);
}

time_t getRtcLocalTime() {
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  struct tm timeinfo;
  log(LogLevel::INFO, "Trying to get current local time...");
  while (!getLocalTime(&timeinfo, 1000)) {
    Serial.printf(".");
  }
  Serial.printf("\n");
  time_t time = mktime(&timeinfo);
  Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return time;
}

void wakeupLight(WakeupFlag *wakeupType, unsigned int *wakeupCount, GxEPD_Class *display, ESP32Time *rtc, Preferences *preferences) {
  log(LogLevel::INFO, "WAKEUP_LIGHT");
  setCpuFrequencyMhz(80);

  time_t time = getRtcLocalTime();

  drawHomeUI(display, rtc, calculateBatteryStatus(), false);
  if (isBatteryCharging(display, preferences)) {
    log(LogLevel::INFO, "Battery is charging!");
    *wakeupType = WakeupFlag::WAKEUP_CHARGING;
  }
  display->update();
  display->powerDown();

  preferences->putLong64("prev_time_unix", time);

  wakeupCount++;

  log(LogLevel::INFO, "Going to sleep...");
  digitalWrite(PWR_EN, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_KEY, 0);
  esp_sleep_enable_timer_wakeup(UPDATE_WAKEUP_TIMER_US);
  esp_deep_sleep_start();
}

void wakeupFull(WakeupFlag *wakeupType, unsigned int *wakeupCount, GxEPD_Class *display, ESP32Time *rtc, Preferences *preferences) {
  log(LogLevel::INFO, "WAKEUP_FULL");
  setCpuFrequencyMhz(240);

  wakeupCount = 0;

  initApps();
  log(LogLevel::SUCCESS, "Apps initiliazed");

  display->fillScreen(GxEPD_WHITE);
  display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT);
}

void wakeupCharging(WakeupFlag *wakeupType, unsigned int *wakeupCount, GxEPD_Class *display, ESP32Time *rtc, Preferences *preferences) {
  bool isCharging;
  do {
    log(LogLevel::INFO, "WAKEUP_CHARGING");
    setCpuFrequencyMhz(240);

    time_t time = getRtcLocalTime();

    drawHomeUI(display, rtc, calculateBatteryStatus(), true);
    isCharging = isBatteryCharging(display, preferences);
    display->update();

    delay(UPDATE_WAKEUP_TIMER_US / 1000 - 5 * 1000);
  } while (isCharging);
  *wakeupType = WakeupFlag::WAKEUP_LIGHT;
  esp_sleep_enable_timer_wakeup(1000000);
  esp_deep_sleep_start();
}

// Loop

void wakeupInitLoop(WakeupFlag *wakeupType, unsigned int sleepTimer, GxEPD_Class *display, ESP32Time *rtc) {
  if (sleepTimer == 30) {
    // if (isBatteryCharging(display))
    //   *wakeupType = WakeupFlag::WAKEUP_CHARGING;
    // else
    *wakeupType = WakeupFlag::WAKEUP_LIGHT;
    esp_sleep_enable_timer_wakeup(1000000);
    esp_deep_sleep_start();
  }
}

void wakeupLightLoop(WakeupFlag *wakeupType, unsigned int sleepTimer, GxEPD_Class *display, ESP32Time *rtc) {
  if (sleepTimer == 15) {
    digitalWrite(PWR_EN, LOW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_KEY, 0);
    esp_sleep_enable_timer_wakeup(UPDATE_WAKEUP_TIMER_US - 15000000);
    esp_deep_sleep_start();
  }
}

void wakeupFullLoop(WakeupFlag *wakeupType, unsigned int sleepTimer, GxEPD_Class *display, ESP32Time *rtc, AwakeState awakeState) {
  if (awakeState == AwakeState::APPS_MENU) {
    drawAppsListUI(display, rtc, calculateBatteryStatus());
    display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT);
  } else {
    apps[currentAppIndex]->drawUI(display);
  }

  if (sleepTimer == 15) {
    *wakeupType = WakeupFlag::WAKEUP_LIGHT;
    esp_sleep_enable_timer_wakeup(1000000);
    esp_deep_sleep_start();
  }
}