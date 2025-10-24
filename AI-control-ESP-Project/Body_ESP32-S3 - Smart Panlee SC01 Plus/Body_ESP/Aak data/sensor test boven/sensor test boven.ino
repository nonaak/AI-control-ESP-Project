#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// I2C pinnen
#define OLED_SDA        21
#define OLED_SCL        22
#define BUS_SDA          4     // sensor-bus (Wire1)
#define BUS_SCL         33

const int BUZZER = 3;

// Sensor thresholds en calibratie
int gsr_threshold = 0;
float flex_baseline = 0;

// NTC constanten (10kΩ @ 25°C, B=3950K)
#define R_TOP 33000.0      // 33kΩ serie weerstand
#define NTC_NOMINAL 10000.0
#define TEMP_NOMINAL 25.0
#define B_COEFFICIENT 3950.0

// Pulse sensor variabelen
#define PULSE_THRESHOLD 100
int pulse_baseline = 0;
unsigned long lastBeat = 0;
int BPM = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  delay(1000);
  
  // I2C sensor bus initialiseren
  Wire1.begin(BUS_SDA, BUS_SCL);
  
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 niet gevonden!");
    while (1);
  }
  
  Serial.println("ADS1115 gevonden!");
  ads.setGain(GAIN_ONE);              // ±4.096V
  ads.setDataRate(RATE_ADS1115_128SPS); // 128 samples/sec
  
  Serial.println("\n=== CALIBRATIE ===");
  Serial.println("Raak sensoren NIET aan...");
  delay(2000);
  
  // GSR calibratie (A0)
  long gsr_sum = 0;
  for(int i = 0; i < 500; i++) {
    gsr_sum += ads.readADC_SingleEnded(0);
    delay(5);
  }
  gsr_threshold = gsr_sum / 500;
  Serial.print("GSR threshold = ");
  Serial.println(gsr_threshold);
  
  // Flex baseline (A1)
  long flex_sum = 0;
  for(int i = 0; i < 100; i++) {
    flex_sum += ads.readADC_SingleEnded(1);
    delay(10);
  }
  flex_baseline = ads.computeVolts(flex_sum / 100);
  Serial.print("Flex baseline = ");
  Serial.print(flex_baseline, 3);
  Serial.println("V");
  
  // Pulse baseline (A2)
  long pulse_sum = 0;
  for(int i = 0; i < 100; i++) {
    pulse_sum += ads.readADC_SingleEnded(2);
    delay(10);
  }
  pulse_baseline = pulse_sum / 100;
  Serial.print("Pulse baseline = ");
  Serial.println(pulse_baseline);
  
  Serial.println("\n=== KLAAR ===");
  Serial.println("A0=GSR | A1=Flex | A2=Pulse | A3=Temp");
  Serial.println();
}

void loop() {
  // ===== A0: GSR Sensor =====
  int gsr_raw = ads.readADC_SingleEnded(0);
  float gsr_voltage = ads.computeVolts(gsr_raw);
  int gsr_diff = abs(gsr_threshold - gsr_raw);
  
  // ===== A1: Flex Sensor (Ademhaling) =====
  int flex_raw = ads.readADC_SingleEnded(1);
  float flex_voltage = ads.computeVolts(flex_raw);
  float breath_value = 100 - ((flex_voltage - 0.5) / 2.0 * 100);
  if(breath_value < 0) breath_value = 0;
  if(breath_value > 100) breath_value = 100;
  
  // ===== A2: Pulse Sensor =====
  int pulse_raw = ads.readADC_SingleEnded(2);
  float pulse_voltage = ads.computeVolts(pulse_raw);
  int pulse_diff = pulse_raw - pulse_baseline;
  
  static int pulse_max = 0;
  static int pulse_min = 32767;
  static bool beat_detected = false;
  
  if(pulse_raw > pulse_max) pulse_max = pulse_raw;
  if(pulse_raw < pulse_min) pulse_min = pulse_raw;
  
  int pulse_threshold_val = pulse_min + ((pulse_max - pulse_min) / 2);
  
  if(pulse_raw > pulse_threshold_val && !beat_detected) {
    unsigned long now = millis();
    if(lastBeat > 0) {
      unsigned long ibi = now - lastBeat;
      if(ibi > 300 && ibi < 2000) {
        BPM = 60000 / ibi;
      }
    }
    lastBeat = now;
    beat_detected = true;
  }
  if(pulse_raw < pulse_threshold_val) {
    beat_detected = false;
  }
  
  static unsigned long lastReset = 0;
  if(millis() - lastReset > 2000) {
    pulse_max = pulse_baseline;
    pulse_min = pulse_baseline;
    lastReset = millis();
  }
  
  // ===== A3: NTC Temperatuur =====
  int ntc_raw = ads.readADC_SingleEnded(3);
  float ntc_voltage = ads.computeVolts(ntc_raw);
  
  Serial.print("GSR: ");
  Serial.print(gsr_voltage, 3);
  Serial.print("V (");
  Serial.print(gsr_diff);
  Serial.print(") | Flex: ");
  Serial.print(flex_voltage, 3);
  Serial.print("V (");
  Serial.print((int)breath_value);
  Serial.print("%) | Pulse: ");
  Serial.print(pulse_voltage, 3);
  Serial.print("V (");
  Serial.print(BPM);
  Serial.print(" BPM)");
  
  // NTC debug en berekening
  Serial.print(" | NTC: raw=");
  Serial.print(ntc_raw);
  Serial.print(" V=");
  Serial.print(ntc_voltage, 3);
  
  if(ntc_voltage < 0.1) {
    Serial.println(" | Temp: OPEN");
  } else if(ntc_voltage > 3.2) {
    Serial.println(" | Temp: SHORT");
  } else {
    float ntc_resistance = R_TOP * ntc_voltage / (3.3 - ntc_voltage);
    
    Serial.print(" R=");
    Serial.print(ntc_resistance, 0);
    Serial.print("Ω");
    
    float steinhart = ntc_resistance / NTC_NOMINAL;
    steinhart = log(steinhart);
    steinhart /= B_COEFFICIENT;
    steinhart += 1.0 / (TEMP_NOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    float temperature = steinhart - 273.15;
    
    Serial.print(" | Temp: ");
    Serial.print(temperature, 1);
    Serial.println("°C");
  }
  
  // GSR alarm
  if(gsr_diff > 200) {
    digitalWrite(BUZZER, HIGH);
    Serial.println(">>> GSR ALARM!");
    delay(500);
    digitalWrite(BUZZER, LOW);
  }
  
  delay(100);
}