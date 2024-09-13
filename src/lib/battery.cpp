#include "battery.h"

#define VOLTAGE_REFERENCE     3.3   // Voltage reference of the ADC
#define ADC_RESOLUTION        4096  // 12-bit ADC resolution
#define NUM_SAMPLES           25    // Number of samples to average
#define DELAY_BETWEEN_SAMPLES 5     // Delay between ADC samples in milliseconds
#define VOLTAGE_RISING_DIFF   0.046 // if the voltage diff is more than this it's probably fine to assume that it's growing
#define VOLTAGE_STABLE_DIFF   0.012 // if the voltage diff is less than this it's probably fine to assume that it's nearing at 100%
#define RESISTOR_MULTIPLIER   2
#define BATTERY_MIN_STATUS    0   // 0%
#define BATTERY_MAX_STATUS    100 // 100%

float readBatteryVoltage() {
  int adcValueSum = 0;

  // Take multiple ADC readings to average out noise
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcValueSum += analogRead(BAT_ADC);
    delay(DELAY_BETWEEN_SAMPLES); // Wait between readings
  }

  // Calculate the average ADC value
  int adcValueAvg = adcValueSum / NUM_SAMPLES;

  // Convert the average ADC value to actual voltage
  float batteryVoltage = (adcValueAvg * VOLTAGE_REFERENCE) / ADC_RESOLUTION;

  // Return the value multiplied by the resistor effective multiplier
  return batteryVoltage * RESISTOR_MULTIPLIER;
}

void setPreviousVoltage(Preferences *preferences) { preferences->putFloat("prev_volt", readBatteryVoltage()); }

int calculateBatteryStatus() {
  return constrain(map(readBatteryVoltage() * 1000, BATTERY_MIN_VOLTAGE, BATTERY_MAX_VOLTAGE, BATTERY_MIN_STATUS, BATTERY_MAX_STATUS),
                   BATTERY_MIN_STATUS, BATTERY_MAX_STATUS);
}

bool isBatteryCharging(GxEPD_Class *display, Preferences *preferences) {
  float currentVoltage = readBatteryVoltage();
  float previousVoltage = preferences->getFloat("prev_volt");
  Serial.printf("The current voltage is: %f\n", currentVoltage);
  Serial.printf("The previous voltage is: %f\n", previousVoltage);
  preferences->putFloat("prev_volt", currentVoltage);

  //  code for debugging
  //  char cur[10];
  //  char pre[10];
  //  dtostrf(currentVoltage, 1, 6, cur);
  //  dtostrf(previousVoltage, 1, 6, pre);
  //  display->setTextColor(GxEPD_BLACK);
  //  display->setFont(&Outfit_60011pt7b);
  //  printLeftString(display, cur, 34, 154);
  //  printLeftString(display, pre, 34, 174);

  float voltDiff = currentVoltage - previousVoltage;
  bool isCharging = voltDiff > VOLTAGE_RISING_DIFF || voltDiff > 0;
  bool isFalling = voltDiff < -VOLTAGE_RISING_DIFF;
  bool wasCharging = preferences->getBool("charging");

  if (wasCharging && isFalling) {
    isCharging = false;
    preferences->putBool("charging", isCharging);
    return isCharging;
  }

  if (wasCharging && !isCharging && !isFalling) {
    isCharging = true;
    preferences->putBool("charging", isCharging);
    return isCharging;
  }

  if (!wasCharging && isCharging) {
    isCharging = voltDiff > VOLTAGE_STABLE_DIFF;
    preferences->putBool("charging", isCharging);
    return isCharging;
  }

  // if (!wasCharging && isCharging)
  //   isCharging = voltageDiff >= VOLTAGE_RISING_DIFF;
  // if (wasCharging && !isCharging)
  //   isCharging = (-voltageDiff < VOLTAGE_STABLE_DIFF);
  // wasCharging = isCharging;

  preferences->putBool("charging", isCharging);
  return isCharging;
}