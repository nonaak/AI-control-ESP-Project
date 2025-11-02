#include "ads1115_sensors.h"
#include <Wire.h>

// ===== GLOBALE OBJECTEN =====
Adafruit_ADS1115 ads;

// ===== INTERNE STAAT =====
static ADS1115_SensorData sensorData;
static bool ads1115Initialized = false;

// Pulse detection variabelen
static int pulseMax = 0;
static int pulseMin = 32767;
static bool beatDetectedFlag = false;
static unsigned long lastBeatTime = 0;
static unsigned long lastPulseReset = 0;

// GSR smoothing
static const float GSR_SMOOTH_FACTOR = 0.2f;  // 0.0-1.0, hogere waarde = trager

// ===== INITIALISATIE =====
bool ads1115_begin() {
  Serial.println("[ADS1115] Initializing...");
  
  // SC01 Plus: Wire1 is al geïnitialiseerd in setup() op GPIO 10/11
  // We gebruiken Wire1 (sensoren zijn op aparte I2C bus)
  
  if (!ads.begin(ADS1115_ADDR, &Wire1)) {
    Serial.println("[ADS1115] ERROR: Not found at address 0x48!");
    return false;
  }
  
  // Configureer ADS1115
  ads.setGain(GAIN_ONE);  // ±4.096V range (voor 3.3V systeem)
  ads.setDataRate(RATE_ADS1115_128SPS);  // 128 samples/sec
  
  Serial.println("[ADS1115] ✓ Initialized successfully");
  Serial.println("[ADS1115] ✓ Gain: ±4.096V");
  Serial.println("[ADS1115] ✓ Sample rate: 128 SPS");
  
  // Initialiseer sensor data
  memset(&sensorData, 0, sizeof(sensorData));
  sensorData.gsrBaseline = 2048.0f;   // Midden van 16-bit bereik
  sensorData.flexBaseline = 1.5f;     // ~1.5V baseline voor flex
  sensorData.pulseBaseline = 2048;    // Midden van bereik
  sensorData.ntcOffset = 0.0f;
  
  ads1115Initialized = true;
  return true;
}

// ===== ALLE SENSOREN LEZEN =====
void ads1115_readAll() {
  if (!ads1115Initialized) return;
  
  // Lees alle 4 ADC kanalen
  sensorData.gsrRaw = ads.readADC_SingleEnded(0);
  sensorData.flexRaw = ads.readADC_SingleEnded(1);
  sensorData.pulseRaw = ads.readADC_SingleEnded(2);
  sensorData.ntcRaw = ads.readADC_SingleEnded(3);
  
  // Converteer naar voltages
  sensorData.gsrVolts = ads.computeVolts(sensorData.gsrRaw);
  sensorData.flexVolts = ads.computeVolts(sensorData.flexRaw);
  sensorData.pulseVolts = ads.computeVolts(sensorData.pulseRaw);
  sensorData.ntcVolts = ads.computeVolts(sensorData.ntcRaw);
  
  // Verwerk elke sensor
  ads1115_readGSR();
  ads1115_readFlex();
  ads1115_readPulse();
  ads1115_readNTC();
}

// ===== GSR SENSOR (A0) =====
void ads1115_readGSR() {
  // GSR verschil ten opzichte van baseline
  float gsrDiff = abs(sensorData.gsrRaw - sensorData.gsrBaseline);
  
  // Exponential moving average smoothing
  sensorData.gsrSmooth = sensorData.gsrSmooth * (1.0f - GSR_SMOOTH_FACTOR) + 
                         gsrDiff * GSR_SMOOTH_FACTOR;
  
  // Zorg dat waarde positief blijft
  if (sensorData.gsrSmooth < 0) sensorData.gsrSmooth = 0;
}

// ===== FLEX SENSOR (A1) - ADEMHALING =====
void ads1115_readFlex() {
  // Converteer flex voltage naar percentage (0-100%)
  // Flex sensor: ~0.5V gebogen, ~2.5V gestrekt
  // Ademhaling: gestrekt = ingeademd (100%), gebogen = uitgeademd (0%)
  sensorData.breathValue = 100.0f - ((sensorData.flexVolts - 0.5f) / 2.0f * 100.0f);
  
  // Clamp naar 0-100%
  if (sensorData.breathValue < 0) sensorData.breathValue = 0;
  if (sensorData.breathValue > 100) sensorData.breathValue = 100;
}

// ===== PULSE SENSOR (A2) - HARTSLAG =====
void ads1115_readPulse() {
  unsigned long now = millis();
  
  // Track max/min voor threshold berekening
  if (sensorData.pulseRaw > pulseMax) pulseMax = sensorData.pulseRaw;
  if (sensorData.pulseRaw < pulseMin) pulseMin = sensorData.pulseRaw;
  
  // Bereken threshold (midden tussen min en max)
  int pulseThreshold = pulseMin + ((pulseMax - pulseMin) / 2);
  
  // Beat detectie: stijgende flank door threshold
  if (sensorData.pulseRaw > pulseThreshold && !beatDetectedFlag) {
    if (lastBeatTime > 0) {
      unsigned long ibi = now - lastBeatTime;  // Inter-beat interval
      
      // Filter valse beats (te snel of te langzaam)
      if (ibi > 300 && ibi < 2000) {  // 30-200 BPM bereik
        sensorData.BPM = 60000 / ibi;
      }
    }
    lastBeatTime = now;
    beatDetectedFlag = true;
    sensorData.beatDetected = true;
  }
  
  // Reset beat flag bij dalende flank
  if (sensorData.pulseRaw < pulseThreshold) {
    beatDetectedFlag = false;
    sensorData.beatDetected = false;
  }
  
  // Reset min/max elke 2 seconden voor adaptieve threshold
  if (now - lastPulseReset > 2000) {
    pulseMax = sensorData.pulseBaseline;
    pulseMin = sensorData.pulseBaseline;
    lastPulseReset = now;
  }
}

// ===== NTC TEMPERATUUR (A3) =====
void ads1115_readNTC() {
  // Check voor open/short circuit
  if (sensorData.ntcVolts < 0.1f) {
    sensorData.temperature = -99.0f;  // OPEN circuit
    return;
  } else if (sensorData.ntcVolts > 3.2f) {
    sensorData.temperature = 99.0f;   // SHORT circuit
    return;
  }
  
  // Bereken NTC weerstand via spanningsdeler
  // Vout = Vin * (Rntc / (Rtop + Rntc))
  // Rntc = Rtop * Vout / (Vin - Vout)
  float ntcResistance = ADS_R_TOP * sensorData.ntcVolts / (3.3f - sensorData.ntcVolts);
  
  // Steinhart-Hart vergelijking voor NTC temperatuur
  // 1/T = 1/T0 + (1/B) * ln(R/R0)
  float steinhart = ntcResistance / ADS_NTC_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= ADS_B_COEFFICIENT;
  steinhart += 1.0f / (ADS_TEMP_NOMINAL + 273.15f);
  steinhart = 1.0f / steinhart;
  
  // Converteer naar Celsius en pas offset toe
  sensorData.temperature = steinhart - 273.15f + sensorData.ntcOffset;
}

// ===== DATA OPHALEN =====
ADS1115_SensorData ads1115_getData() {
  return sensorData;
}

// ===== DEBUG OUTPUT =====
void ads1115_printDebug() {
  Serial.println("[ADS1115] ===== SENSOR DATA =====");
  Serial.printf("  GSR:   Raw=%d, Volts=%.3fV, Smooth=%.1f\n", 
                sensorData.gsrRaw, sensorData.gsrVolts, sensorData.gsrSmooth);
  Serial.printf("  FLEX:  Raw=%d, Volts=%.3fV, Breath=%.1f%%\n", 
                sensorData.flexRaw, sensorData.flexVolts, sensorData.breathValue);
  Serial.printf("  PULSE: Raw=%d, Volts=%.3fV, BPM=%d, Beat=%s\n", 
                sensorData.pulseRaw, sensorData.pulseVolts, sensorData.BPM, 
                sensorData.beatDetected ? "YES" : "NO");
  Serial.printf("  NTC:   Raw=%d, Volts=%.3fV, Temp=%.1f°C\n", 
                sensorData.ntcRaw, sensorData.ntcVolts, sensorData.temperature);
}

// ===== KALIBRATIE FUNCTIES =====
void ads1115_setGSRBaseline(float baseline) {
  sensorData.gsrBaseline = baseline;
  Serial.printf("[ADS1115] GSR baseline set to %.1f\n", baseline);
}

void ads1115_setFlexBaseline(float baseline) {
  sensorData.flexBaseline = baseline;
  Serial.printf("[ADS1115] Flex baseline set to %.3fV\n", baseline);
}

void ads1115_setPulseBaseline(int baseline) {
  sensorData.pulseBaseline = baseline;
  Serial.printf("[ADS1115] Pulse baseline set to %d\n", baseline);
}

void ads1115_setNTCOffset(float offset) {
  sensorData.ntcOffset = offset;
  Serial.printf("[ADS1115] NTC offset set to %.2f°C\n", offset);
}
