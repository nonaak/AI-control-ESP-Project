/*
  SENSOR SETTINGS - Minimale versie
  
  De EEPROM functies zijn verplaatst naar nvs_settings.h/cpp
  Dit bestand bevat alleen de struct definitie voor compatibiliteit
*/

#ifndef SENSOR_SETTINGS_H
#define SENSOR_SETTINGS_H

#include <Arduino.h>

// ===== Sensor Configuratie Struct =====
// Wordt gebruikt door body_menu.cpp en ads1115_sensors.cpp
struct SensorConfig {
  // Hart sensor
  float hrThreshold;         // Beat detectie drempel
  int ads_pulseBaseline;     // Pulse baseline
  
  // Temperatuur sensor
  float tempOffset;          // Temperatuur offset
  float tempSmoothing;       // Smoothing factor
  float ads_ntcOffset;       // NTC offset
  
  // GSR sensor
  float ads_gsrBaseline;     // GSR baseline
  float gsrSensitivity;      // Gevoeligheid
  float gsrSmoothing;        // Smoothing factor
};

// Globale sensor config (gedefinieerd in ads1115_sensors.cpp)
extern SensorConfig sensorConfig;

// ===== Functies (gedefinieerd in ads1115_sensors.cpp) =====
// Deze functies passen de config toe op de ADS1115
void applySensorConfig();

// De volgende functies zijn DEPRECATED - gebruik nvs_settings in plaats daarvan:
// void loadSensorConfig();   // Vervangen door nvsSettings_loadSensorCal()
// void saveSensorConfig();   // Vervangen door nvsSettings_saveSensorCal()

#endif // SENSOR_SETTINGS_H
