#pragma once
#include <Arduino.h>
#include <OneButton.h>
// #include <esp_adc_cal.h>  // OUDE ESP-IDF ADC API - veroorzaakt conflict

// T-HMI Power Management System
// Gebaseerd op T-HMI-master examples voor accu en aan/uit knop

// ===== T-HMI Power Management Pinnen =====
// // Oude CYD power pinnen - niet gebruikt
// #define OLD_CYD_PWR_PIN   xx  // OUDE CYD POWER PIN
// #define OLD_CYD_BAT_PIN   xx  // OUDE CYD BATTERY PIN

// T-HMI power pinnen (uit T-HMI-master)
#define PWR_EN_PIN  (10)    // T-HMI power enable pin - KRITIEK!
#define PWR_ON_PIN  (14)    // T-HMI power on pin - voor aan/uit knop
#define BAT_ADC_PIN (5)     // T-HMI battery voltage monitoring
#define POWER_BTN_PIN (21)  // T-HMI aan/uit knop

// ===== Battery Management =====
#define BATTERY_LOW_VOLTAGE    3.3f  // Laag battery alarm voltage
#define BATTERY_CRITICAL_VOLTAGE 3.1f  // Kritiek battery voltage (auto shutdown)
#define BATTERY_FULL_VOLTAGE   4.2f  // Vol battery voltage
#define BATTERY_EMPTY_VOLTAGE  3.0f  // Leeg battery voltage

// ===== Sleep Management =====
#define SLEEP_TIMEOUT_MS      (120 * 60 * 1000)  // 30 minuten inactiviteit = sleep
#define DEEP_SLEEP_TIMEOUT_MS (5 * 60 * 1000)   // 5 minuten = deep sleep

// ===== Power States =====
enum PowerState : uint8_t {
  POWER_ON = 0,        // Normaal aan
  POWER_SLEEP,         // Light sleep (touch wake-up)  
  POWER_DEEP_SLEEP,    // Deep sleep (alleen power button wake-up)
  POWER_OFF,           // Volledig uit
  POWER_LOW_BATTERY,   // Low battery modus
  POWER_CRITICAL       // Kritieke battery - emergency shutdown
};

// ===== Power Management Class =====
class THMIPowerManager {
private:
  OneButton powerButton;
  uint32_t lastActivity;
  uint32_t sleepStartTime;
  PowerState currentState;
  bool batteryConnected;
  float lastBatteryVoltage;
  uint32_t lastBatteryCheck;
  
  // Private functies
  uint32_t readAdcVoltage(int pin);
  void enterSleep();
  void enterDeepSleep();
  void emergencyShutdown();
  void updateActivity();

public:
  // Constructor
  THMIPowerManager();
  
  // Initialisatie
  void begin();
  
  // Main loop functie
  void loop();
  
  // Power control
  void powerOff();
  void wakeUp();
  void resetSleepTimer();
  
  // Battery functies
  float getBatteryVoltage();
  int getBatteryPercentage();
  bool isBatteryConnected();
  bool isBatteryLow();
  bool isBatteryCritical();
  
  // Status functies
  PowerState getPowerState();
  bool isActive();
  bool canSleep();
  
  // Callback voor power button
  static void powerButtonClick();
  static void powerButtonLongPress();
};

// Global power manager instance
extern THMIPowerManager powerManager;

// Helper functies voor main code
void initPowerManagement();
void handlePowerManagement();
bool shouldSleep();