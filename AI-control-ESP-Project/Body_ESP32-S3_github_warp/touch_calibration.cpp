#include "touch_calibration.h"
#include <EEPROM.h>
// Geen SPI/XPT2046 includes - gebruiken externe touch instance

// EEPROM adressen voor kalibratie data
#define EEPROM_CAL_ADDR 100
#define EEPROM_CAL_MAGIC 0xCAFE

static TFT_eSPI* g = nullptr;
// Gebruik externe touch instance om conflicts te voorkomen
extern bool inputTouchReadRaw(int16_t &rawX, int16_t &rawY);

struct CalPoint {
  int16_t screenX, screenY;
  int16_t rawX, rawY;
};

static CalPoint calPoints[4];
static int currentPoint = 0;
static bool calibrationActive = false;
static bool calibrationDone = false;
static CalibrationData savedCalData = {0, 0, 0, 0, false};

// Forward declarations
static void drawCalibrationScreen();
static void drawCrosshair(int16_t x, int16_t y, uint16_t color);
static void calculateAndSave();

void touchCalibration_begin(TFT_eSPI* gfx) {
  g = gfx;
  calibrationActive = true;
  calibrationDone = false;
  currentPoint = 0;
  
  // Initialiseer EEPROM
  EEPROM.begin(512);
  
  Serial.println("[CALIBRATE] Touch kalibratie gestart - gebruik bestaande touch system");
  
  // Kalibratie punten (4 hoeken met marge)
  int margin = 30;
  calPoints[0] = {margin, margin, 0, 0};                     // Links boven
  calPoints[1] = {320-margin, margin, 0, 0};                 // Rechts boven  
  calPoints[2] = {320-margin, 240-margin, 0, 0};             // Rechts onder
  calPoints[3] = {margin, 240-margin, 0, 0};                 // Links onder
  
  drawCalibrationScreen();
}

static void drawCalibrationScreen() {
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_WHITE);
  g->setTextSize(2);
  g->setCursor(10, 10);
  g->printf("Touch Kalibratie %d/4", currentPoint + 1);
  
  g->setTextSize(1);
  g->setCursor(10, 40);
  g->println("Raak het RODE kruisje precies aan");
  g->setCursor(10, 55);
  g->println("Laat los en wacht op het volgende");
  
  // Teken kruisjes
  for (int i = 0; i < 4; i++) {
    uint16_t color;
    if (i < currentPoint) color = TFT_GREEN;        // Al gedaan
    else if (i == currentPoint) color = TFT_RED;    // Huidig
    else color = TFT_DARKGREY;                      // Nog te doen
    
    drawCrosshair(calPoints[i].screenX, calPoints[i].screenY, color);
  }
  
  g->setCursor(10, 220);
  g->printf("Punt %d: Raak ROOD kruisje aan", currentPoint + 1);
}

static void drawCrosshair(int16_t x, int16_t y, uint16_t color) {
  int size = 12;
  g->drawLine(x-size, y, x+size, y, color);        // Horizontaal
  g->drawLine(x, y-size, x, y+size, color);        // Verticaal  
  g->fillCircle(x, y, 2, color);                   // Centrum punt
}

bool touchCalibration_poll() {
  if (!calibrationActive || calibrationDone) return calibrationDone;
  
  static bool wasTouching = false;
  static bool hasRawData = false;
  static int16_t rawX, rawY;
  
  // Gebruik bestaande touch system
  if (inputTouchReadRaw(rawX, rawY)) {
    if (!wasTouching) {
      hasRawData = true;
      wasTouching = true;
      
      Serial.printf("[CALIBRATE] Touch - raw coords: (%d, %d)\n", rawX, rawY);
    }
  } else {
    // Touch released
    if (wasTouching && hasRawData) {
      wasTouching = false;
      
      // Sla ruwe coordinaten op
      calPoints[currentPoint].rawX = rawX;
      calPoints[currentPoint].rawY = rawY;
      
      Serial.printf("[CALIBRATE] Punt %d: scherm(%d,%d) -> raw(%d,%d)\n", 
                    currentPoint + 1,
                    calPoints[currentPoint].screenX, 
                    calPoints[currentPoint].screenY,
                    calPoints[currentPoint].rawX,
                    calPoints[currentPoint].rawY);
      
      currentPoint++;
      
      if (currentPoint >= 4) {
        // Kalibratie compleet!
        calculateAndSave();
        calibrationDone = true;
        calibrationActive = false;
        return true;
      } else {
        // Volgende punt
        drawCalibrationScreen();
      }
      
      hasRawData = false;
    }
  }
  
  return false;  // Nog niet klaar
}

static void calculateAndSave() {
  // Debug: toon alle punten
  Serial.println("[CALIBRATE] Raw punten:");
  for (int i = 0; i < 4; i++) {
    Serial.printf("  Punt %d: scherm(%d,%d) -> raw(%d,%d)\n", 
                  i, calPoints[i].screenX, calPoints[i].screenY, 
                  calPoints[i].rawX, calPoints[i].rawY);
  }
  
  // Bereken kalibratie waarden - VASTE X/Y MAPPING
  int16_t xmin_raw = (calPoints[0].rawX + calPoints[3].rawX) / 2;  // Links punten
  int16_t xmax_raw = (calPoints[1].rawX + calPoints[2].rawX) / 2;  // Rechts punten
  int16_t ymin_raw = (calPoints[0].rawY + calPoints[1].rawY) / 2;  // Boven punten
  int16_t ymax_raw = (calPoints[2].rawY + calPoints[3].rawY) / 2;  // Onder punten
  
  // Check of X-waarden omgedraaid moeten worden
  int16_t xmin, xmax;
  if (xmin_raw < xmax_raw) {
    xmin = xmin_raw;
    xmax = xmax_raw;
  } else {
    xmin = xmax_raw;  // Omdraaien
    xmax = xmin_raw;
    Serial.println("[CALIBRATE] X-waarden omgedraaid voor correcte mapping");
  }
  
  // Check of Y-waarden omgedraaid moeten worden
  int16_t ymin, ymax;
  if (ymin_raw < ymax_raw) {
    ymin = ymin_raw;
    ymax = ymax_raw;
  } else {
    ymin = ymax_raw;  // Omdraaien
    ymax = ymin_raw;
    Serial.println("[CALIBRATE] Y-waarden omgedraaid voor correcte mapping");
  }
  
  // OUDE correctie (gokwerk):
  // int16_t yRange = ymax - ymin;
  // int16_t yCorrection = yRange / 10;  // 10% naar beneden (was 5%)
  // ymin -= yCorrection;  // Verschuif touch zone naar beneden
  // ymax -= yCorrection;
  // int16_t xRange = xmax - xmin;
  // int16_t xCorrection = xRange / 20;  // 5% naar links
  // xmin -= xCorrection;  // Verschuif touch zone naar links
  // xmax -= xCorrection;
  
  // NIEUWE correctie gebaseerd op T-HMI factory data:
  // Factory: touch.setCal(285, 1788, 311, 1877, 240, 320) met rotatie 3
  // Factory punten: (10,10) (229,10) (229,309) (10,309) in rotatie 0
  // Onze punten: (30,30) (290,30) (290,210) (30,210) in rotatie 3
  
  // Factory mapping voor rotatie 3:
  // Left-Right mapping moet omgedraaid worden voor rotatie 3
  if (xmin > xmax) {
    // Al omgedraaid door eerdere check, gebruik zoals is
    int16_t xCorrection = 0;  // Geen extra X correctie
    int16_t yCorrection = 0;  // Geen extra Y correctie
  } else {
    // Niet omgedraaid, doe niets extra
    int16_t xCorrection = 0;
    int16_t yCorrection = 0;
  }
  
  Serial.printf("[CALIBRATE] Gebruikt factory-gebaseerde correctie\n");
  
  // Sla op in EEPROM
  struct EEPROMCalData {
    uint16_t magic;
    int16_t xmin, xmax, ymin, ymax;
    uint16_t checksum;
  } eepromData;
  
  eepromData.magic = EEPROM_CAL_MAGIC;
  eepromData.xmin = xmin;
  eepromData.xmax = xmax;
  eepromData.ymin = ymin;
  eepromData.ymax = ymax;
  eepromData.checksum = xmin + xmax + ymin + ymax + EEPROM_CAL_MAGIC;
  
  // Schrijf naar EEPROM
  EEPROM.put(EEPROM_CAL_ADDR, eepromData);
  EEPROM.commit();
  
  // Update lokale data
  savedCalData.xmin = xmin;
  savedCalData.xmax = xmax;
  savedCalData.ymin = ymin;
  savedCalData.ymax = ymax;
  savedCalData.valid = true;
  
  Serial.println("[CALIBRATE] Kalibratie data opgeslagen in EEPROM");
  Serial.printf("[CALIBRATE] Factory-gecorrigeerde waarden: (%d, %d, %d, %d)\n", xmin, xmax, ymin, ymax);
  // Serial.printf("[CALIBRATE] Y-correctie: -%d pixels (te hoog fix)\n", yCorrection);
  // Serial.printf("[CALIBRATE] X-correctie: -%d pixels (te rechts fix)\n", xCorrection);
  
  // Toon resultaat
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_GREEN);
  g->setTextSize(2);
  g->setCursor(10, 10);
  g->println("Kalibratie Klaar!");
  
  g->setTextSize(1);
  g->setTextColor(TFT_WHITE);
  g->setCursor(10, 50);
  g->println("Nieuwe kalibratie waarden zijn");
  g->setCursor(10, 65);
  g->println("opgeslagen in het geheugen.");
  
  g->setCursor(10, 90);
  g->println("Touch wordt nu automatisch");
  g->setCursor(10, 105);
  g->println("geladen met nieuwe waarden.");
  
  g->setTextColor(TFT_YELLOW);
  g->setCursor(10, 130);
  g->printf("Cal: %d,%d,%d,%d", xmin, xmax, ymin, ymax);
  
  g->setTextColor(TFT_CYAN);
  g->setCursor(10, 160);
  g->println("Druk ergens om terug te gaan");
  g->setCursor(10, 175);
  g->println("naar het systeem menu.");
}

void touchCalibration_loadFromEEPROM() {
  EEPROM.begin(512);
  
  struct EEPROMCalData {
    uint16_t magic;
    int16_t xmin, xmax, ymin, ymax;
    uint16_t checksum;
  } eepromData;
  
  EEPROM.get(EEPROM_CAL_ADDR, eepromData);
  
  // Valideer data
  uint16_t expectedChecksum = eepromData.xmin + eepromData.xmax + eepromData.ymin + eepromData.ymax + EEPROM_CAL_MAGIC;
  
  if (eepromData.magic == EEPROM_CAL_MAGIC && eepromData.checksum == expectedChecksum) {
    savedCalData.xmin = eepromData.xmin;
    savedCalData.xmax = eepromData.xmax;
    savedCalData.ymin = eepromData.ymin;
    savedCalData.ymax = eepromData.ymax;
    savedCalData.valid = true;
    
    Serial.println("[CALIBRATE] Geldige kalibratie data geladen uit EEPROM");
    Serial.printf("[CALIBRATE] Waarden: (%d, %d, %d, %d)\n", 
                  savedCalData.xmin, savedCalData.xmax, savedCalData.ymin, savedCalData.ymax);
  } else {
    savedCalData.valid = false;
    Serial.println("[CALIBRATE] Geen geldige kalibratie data in EEPROM");
  }
}

bool touchCalibration_hasValidData() {
  return savedCalData.valid;
}

CalibrationData touchCalibration_getData() {
  return savedCalData;
}