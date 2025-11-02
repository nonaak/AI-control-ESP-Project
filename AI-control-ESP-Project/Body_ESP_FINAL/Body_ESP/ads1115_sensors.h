#pragma once
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

/*
 * ADS1115 Sensor Management voor Body ESP ESP32-S3
 * 
 * 4 Kanalen:
 * A0 = Grove GSR (Huidgeleiding/Stress)
 * A1 = Flex Sensor (Ademhaling)
 * A2 = Pulse Sensor (PPG - Hartslag)
 * A3 = NTC 10kΩ (Temperatuur)
 * 
 * I2C Pinnen ESP32-S3:
 * SDA = 4
 * SCL = 33
 * 
 * I2C Adres: 0x48 (standaard ADS1115)
 */

// ===== NTC THERMISTOR CONSTANTEN =====
#define ADS_R_TOP           10000.0  // 10kΩ serie weerstand voor NTC
#define ADS_NTC_NOMINAL     10000.0  // 10kΩ bij 25°C
#define ADS_TEMP_NOMINAL    25.0     // Nominale temperatuur
#define ADS_B_COEFFICIENT   3950.0   // B-coefficient van NTC

// ===== ADS1115 CONFIGURATIE (SC01 Plus - Wire1) =====
#define ADS1115_ADDR        0x48
#define ADS1115_SDA         10  // SC01 Plus: GPIO 10 (Wire1)
#define ADS1115_SCL         11  // SC01 Plus: GPIO 11 (Wire1)

// ===== SENSOR DATA STRUCTUUR =====
struct ADS1115_SensorData {
  // Raw ADC waarden (16-bit signed)
  int16_t gsrRaw;
  int16_t flexRaw;
  int16_t pulseRaw;
  int16_t ntcRaw;
  
  // Voltage waarden (0-4.096V)
  float gsrVolts;
  float flexVolts;
  float pulseVolts;
  float ntcVolts;
  
  // Verwerkte waarden
  float gsrSmooth;        // Geëgaliseerde GSR waarde
  float breathValue;      // Ademhaling 0-100%
  uint16_t BPM;           // Hartslag in BPM
  float temperature;      // Temperatuur in °C
  bool beatDetected;      // Hartslag beat gedetecteerd
  
  // Kalibratie
  float gsrBaseline;      // GSR baseline voor verschil
  float flexBaseline;     // Flex sensor baseline
  int pulseBaseline;      // Pulse sensor baseline
  float ntcOffset;        // NTC temperatuur offset
};

// ===== FUNCTIE PROTOTYPES =====

// Initialisatie
bool ads1115_begin();

// Sensor lezen
void ads1115_readAll();
void ads1115_readGSR();
void ads1115_readFlex();
void ads1115_readPulse();
void ads1115_readNTC();

// Data ophalen
ADS1115_SensorData ads1115_getData();
void ads1115_printDebug();

// Kalibratie
void ads1115_setGSRBaseline(float baseline);
void ads1115_setFlexBaseline(float baseline);
void ads1115_setPulseBaseline(int baseline);
void ads1115_setNTCOffset(float offset);

// Externe ADS1115 object
extern Adafruit_ADS1115 ads;
