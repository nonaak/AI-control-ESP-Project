#include "t_hmi_power.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// Global power manager instance
THMIPowerManager powerManager;

// Static instance pointer voor callbacks
static THMIPowerManager* instancePtr = nullptr;

// ===== Constructor =====
THMIPowerManager::THMIPowerManager() : powerButton(POWER_BTN_PIN, false, false) {
  lastActivity = 0;
  sleepStartTime = 0;
  currentState = POWER_ON;
  batteryConnected = false;
  lastBatteryVoltage = 0.0f;
  lastBatteryCheck = 0;
  instancePtr = this;
}

// ===== T-HMI Power Initialisatie =====
void THMIPowerManager::begin() {
  Serial.println("[T-HMI POWER] Initialiseer power management systeem...");
  
  // KRITIEK: T-HMI power enable - MOET als eerste!
  pinMode(PWR_ON_PIN, OUTPUT);
  digitalWrite(PWR_ON_PIN, HIGH);
  Serial.println("[T-HMI POWER] PWR_ON_PIN hoog gezet - systeem blijft aan");
  
  // Power enable voor peripherals
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, HIGH);
  Serial.println("[T-HMI POWER] PWR_EN_PIN hoog gezet - peripherals aan");
  
  // Power button setup met callbacks
  powerButton.attachClick(powerButtonClick);
  powerButton.attachLongPressStop(powerButtonLongPress);
  Serial.println("[T-HMI POWER] Power button callbacks ingesteld");
  
  // Battery ADC pin setup
  pinMode(BAT_ADC_PIN, INPUT);
  Serial.println("[T-HMI POWER] Battery ADC pin ingesteld");
  
  // Eerste battery check
  float voltage = getBatteryVoltage();
  batteryConnected = (voltage > BATTERY_EMPTY_VOLTAGE);
  
  if (batteryConnected) {
    Serial.printf("[T-HMI POWER] Battery gedetecteerd: %.2fV (%d%%)\n", 
                  voltage, getBatteryPercentage());
  } else {
    Serial.println("[T-HMI POWER] Geen battery gedetecteerd - USB power");
  }
  
  // Activity timer starten
  lastActivity = millis();
  Serial.println("[T-HMI POWER] Power management gereed!");
}

// ===== Main Loop =====
void THMIPowerManager::loop() {
  // Power button handling
  powerButton.tick();
  
  // Battery monitoring (elke 30 seconden)
  uint32_t now = millis();
  if (now - lastBatteryCheck > 30000) {
    float voltage = getBatteryVoltage();
    
    // Check voor kritieke battery
    if (batteryConnected && voltage < BATTERY_CRITICAL_VOLTAGE) {
      Serial.printf("[T-HMI POWER] KRITIEKE BATTERY: %.2fV - Emergency shutdown!\n", voltage);
      currentState = POWER_CRITICAL;
      emergencyShutdown();
      return;
    }
    
    // Check voor lage battery
    if (batteryConnected && voltage < BATTERY_LOW_VOLTAGE && currentState != POWER_LOW_BATTERY) {
      Serial.printf("[T-HMI POWER] LAGE BATTERY: %.2fV (%d%%)\n", voltage, getBatteryPercentage());
      currentState = POWER_LOW_BATTERY;
      // TODO: Toon low battery waarschuwing op scherm
    }
    
    lastBatteryVoltage = voltage;
    lastBatteryCheck = now;
  }
  
  // Sleep management
  if (currentState == POWER_ON || currentState == POWER_LOW_BATTERY) {
    uint32_t inactiveTime = now - lastActivity;
    
    // Deep sleep na lange inactiviteit (alleen bij battery power)
    if (batteryConnected && inactiveTime > SLEEP_TIMEOUT_MS) {
      Serial.println("[T-HMI POWER] Lange inactiviteit - ga naar deep sleep voor battery besparing");
      enterDeepSleep();
    }
    // Light sleep na korte inactiviteit
    else if (inactiveTime > (SLEEP_TIMEOUT_MS / 4)) {
      // TODO: Dim backlight voor power saving
      // digitalWrite(BK_LIGHT_PIN, LOW);
    }
  }
}

// ===== Battery Voltage Reading (Arduino ADC - geen conflict) =====
uint32_t THMIPowerManager::readAdcVoltage(int pin) {
  // Gebruik eenvoudige Arduino analogRead - geen oude API conflict
  int rawValue = analogRead(pin);
  // ESP32-S3 ADC: 12-bit (0-4095) met 3.3V referentie
  // Approximatie: 1 ADC unit ≈ 0.8mV (3300mV / 4095)
  return (rawValue * 3300) / 4095;  // Convert naar millivolts
}

float THMIPowerManager::getBatteryVoltage() {
  // T-HMI heeft voltage divider - vermenigvuldig met 2 (uit T-HMI example)
  uint32_t voltage = readAdcVoltage(BAT_ADC_PIN) * 2;
  return (float)voltage / 1000.0f;  // Convert naar volts
}

int THMIPowerManager::getBatteryPercentage() {
  if (!batteryConnected) return 100;  // USB power = 100%
  
  float voltage = lastBatteryVoltage > 0 ? lastBatteryVoltage : getBatteryVoltage();
  
  // Linear mapping van battery voltage naar percentage
  if (voltage >= BATTERY_FULL_VOLTAGE) return 100;
  if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0;
  
  float percentage = ((voltage - BATTERY_EMPTY_VOLTAGE) / 
                     (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0f;
  return constrain((int)percentage, 0, 100);
}

bool THMIPowerManager::isBatteryConnected() {
  return batteryConnected;
}

bool THMIPowerManager::isBatteryLow() {
  return (currentState == POWER_LOW_BATTERY);
}

bool THMIPowerManager::isBatteryCritical() {
  return (currentState == POWER_CRITICAL);
}

// ===== Sleep Functions =====
void THMIPowerManager::enterSleep() {
  Serial.println("[T-HMI POWER] Ga naar light sleep...");
  currentState = POWER_SLEEP;
  sleepStartTime = millis();
  
  // Configureer wake-up bronnen
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);  // Power button wake-up
  // esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);  // Touch wake-up
  
  // Light sleep - behoud RAM
  esp_light_sleep_start();
  
  // Wake-up
  Serial.println("[T-HMI POWER] Wake-up uit light sleep");
  wakeUp();
}

void THMIPowerManager::enterDeepSleep() {
  Serial.println("[T-HMI POWER] Ga naar deep sleep voor battery besparing...");
  currentState = POWER_DEEP_SLEEP;
  
  // Schakel peripherals uit voor maximum power saving
  digitalWrite(PWR_EN_PIN, LOW);  // Peripherals uit
  // digitalWrite(BK_LIGHT_PIN, LOW);  // Backlight uit
  
  // Configureer alleen power button wake-up
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);  // Power button (GPIO 21)
  
  Serial.println("[T-HMI POWER] Deep sleep - alleen power button kan wakker maken");
  
  // Deep sleep - RAM verloren, volledige reset bij wake-up
  esp_deep_sleep_start();
}

void THMIPowerManager::emergencyShutdown() {
  Serial.println("[T-HMI POWER] EMERGENCY SHUTDOWN - kritieke battery!");
  
  // TODO: Toon emergency bericht op scherm
  // TODO: Save belangrijke data naar flash
  
  delay(2000);  // Tijd om bericht te lezen
  
  // Volledige shutdown
  digitalWrite(PWR_ON_PIN, LOW);   // Hoofdpower uit
  digitalWrite(PWR_EN_PIN, LOW);   // Peripherals uit
  
  // Als dat niet werkt, deep sleep zonder wake-up
  esp_deep_sleep_start();
}

void THMIPowerManager::wakeUp() {
  if (currentState == POWER_SLEEP || currentState == POWER_DEEP_SLEEP) {
    Serial.println("[T-HMI POWER] Wake-up gedetecteerd");
    
    // Peripherals weer aan
    digitalWrite(PWR_EN_PIN, HIGH);
    // digitalWrite(BK_LIGHT_PIN, HIGH);
    
    currentState = POWER_ON;
    updateActivity();
  }
}

// ===== Power Button Callbacks =====
void THMIPowerManager::powerButtonClick() {
  if (instancePtr) {
    Serial.println("[T-HMI POWER] Power button kort ingedrukt");
    instancePtr->updateActivity();
    
    if (instancePtr->currentState == POWER_SLEEP) {
      instancePtr->wakeUp();
    }
  }
}

void THMIPowerManager::powerButtonLongPress() {
  if (instancePtr) {
    Serial.println("[T-HMI POWER] Power button lang ingedrukt - SHUTDOWN");
    instancePtr->powerOff();
  }
}

void THMIPowerManager::powerOff() {
  Serial.println("[T-HMI POWER] Shutdown gestart...");
  currentState = POWER_OFF;
  
  // TODO: Save data, stop recordings, etc.
  
  delay(1000);  // Tijd voor cleanup
  
  // T-HMI shutdown sequence
  digitalWrite(PWR_ON_PIN, LOW);   // Hoofdpower uit - dit schakelt T-HMI uit
  digitalWrite(PWR_EN_PIN, LOW);   // Peripherals uit
  
  Serial.println("[T-HMI POWER] System uit - power button om weer aan te zetten");
}

// ===== Activity Management =====
void THMIPowerManager::updateActivity() {
  lastActivity = millis();
  if (currentState == POWER_LOW_BATTERY) {
    // Blijf in low battery modus tot voltage omhoog gaat
  } else if (currentState != POWER_CRITICAL) {
    currentState = POWER_ON;
  }
}

void THMIPowerManager::resetSleepTimer() {
  updateActivity();
}

// ===== Status Functions =====
PowerState THMIPowerManager::getPowerState() {
  return currentState;
}

bool THMIPowerManager::isActive() {
  return (currentState == POWER_ON || currentState == POWER_LOW_BATTERY);
}

bool THMIPowerManager::canSleep() {
  return (batteryConnected && (millis() - lastActivity) > SLEEP_TIMEOUT_MS);
}

// ===== Helper Functions =====
void initPowerManagement() {
  powerManager.begin();
}

void handlePowerManagement() {
  powerManager.loop();
}

bool shouldSleep() {
  return powerManager.canSleep();
}