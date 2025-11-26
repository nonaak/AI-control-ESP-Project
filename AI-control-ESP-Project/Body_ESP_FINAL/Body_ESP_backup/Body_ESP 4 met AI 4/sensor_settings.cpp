/*
  SENSOR SETTINGS - Minimale implementatie
  
  Definieert de globale sensorConfig struct.
  EEPROM functies zijn verplaatst naar nvs_settings.
  
  LET OP: Als je al een sensorConfig hebt in ads1115_sensors.cpp,
  verwijder dan dit bestand om dubbele definitie te voorkomen!
*/

#include "sensor_settings.h"

// Globale sensor configuratie struct
// Standaard waarden worden overschreven door NVS bij startup
SensorConfig sensorConfig = {
  .hrThreshold = 50000.0f,      // Hart beat drempel
  .ads_pulseBaseline = 0,       // Pulse baseline
  .tempOffset = 0.0f,           // Temp offset
  .tempSmoothing = 0.2f,        // Temp smoothing
  .ads_ntcOffset = 0.0f,        // NTC offset
  .ads_gsrBaseline = 500.0f,    // GSR baseline
  .gsrSensitivity = 1.0f,       // GSR gevoeligheid
  .gsrSmoothing = 0.3f          // GSR smoothing
};

// Pas sensor configuratie toe op ADS1115
// Deze functie roept de ads1115_setXxx functies aan
void applySensorConfig() {
  // Extern functies uit ads1115_sensors.cpp
  extern void ads1115_setGSRBaseline(float baseline);
  extern void ads1115_setNTCOffset(float offset);
  extern void ads1115_setPulseBaseline(int baseline);
  
  ads1115_setGSRBaseline(sensorConfig.ads_gsrBaseline);
  ads1115_setNTCOffset(sensorConfig.ads_ntcOffset);
  ads1115_setPulseBaseline(sensorConfig.ads_pulseBaseline);
  
  Serial.println("[SENSOR] Config applied to ADS1115");
}
