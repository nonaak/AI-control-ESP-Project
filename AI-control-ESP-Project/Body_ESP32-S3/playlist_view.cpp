#include "playlist_view.h"
// FIXED: Verwijderd Arduino_GFX_Library.h en SD.h (oude CYD code)
// FIXED: Gebruik SD_MMC voor T-HMI hardware
#include <SD_MMC.h>
#include "input_touch.h"

// Voeg dit toe bovenaan het bestand, na de #include statements
#ifndef BLACK
#define BLACK 0x0000
#endif

#ifndef WHITE
#define WHITE 0xFFFF
#endif

static TFT_eSPI* g = nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_PLAY=0x07E0;
static const uint16_t COL_BTN_DELETE=0xF800;
static const uint16_t COL_BTN_AI=0xF81F;
static const uint16_t COL_BTN_BACK=0x001F;
static const uint16_t COL_FILE_BG=0x2104;
static const uint16_t COL_FILE_SEL=0x4A69;

static int16_t SCR_W=320, SCR_H=240;
static String files[10];
static int fileCount = 0;
static int selectedFile = 0;

static uint32_t lastTouchMs = 0;
static const uint16_t PLAYLIST_COOLDOWN_MS = 800;

// Button rectangles
static int16_t bxPlayX,bxPlayY,bxPlayW,bxPlayH;
static int16_t bxDelX,bxDelY,bxDelW,bxDelH;
static int16_t bxAiX,bxAiY,bxAiW,bxAiH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

static int16_t fileListY = 60;
static int16_t fileItemH = 20;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

// FIXED: T-HMI SD_MMC pin configuratie
#define SD_MISO_PIN (13)
#define SD_MOSI_PIN (11)
#define SD_SCLK_PIN (12)

static void scanSDFiles() {
  fileCount = 0;
  selectedFile = 0;
  
  // FIXED: Gebruik SD_MMC voor T-HMI met correcte pins
  SD_MMC.setPins(SD_SCLK_PIN, SD_MOSI_PIN, SD_MISO_PIN);
  
  // FIXED: Check of SD_MMC al geïnitialiseerd is in setup()
  // Als het niet werkt, probeer opnieuw te initialiseren
  static bool sdInitialized = false;
  if (!sdInitialized) {
    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("[PLAYLIST] SD_MMC init failed!");
      return;
    }
    sdInitialized = true;
    Serial.println("[PLAYLIST] SD_MMC initialized");
  }
  
  // FIXED: Gebruik SD_MMC in plaats van SD
  //File root = SD_MMC.open("/");
  fs::File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("[PLAYLIST] Failed to open root directory");
    return;
  }
  
  //File file = root.openNextFile();
  fs::File file = root.openNextFile();
  while (file && fileCount < 10) {
    if (!file.isDirectory()) {
      String name = file.name();
      // Strip leading slash als aanwezig (SD_MMC gedrag)
      if (name.startsWith("/")) {
        name = name.substring(1);
      }
      
      if (name.startsWith("data") && name.endsWith(".csv")) {
        files[fileCount] = name;
        fileCount++;
        Serial.printf("[PLAYLIST] Found: %s\n", name.c_str());
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
  Serial.printf("[PLAYLIST] Total files found: %d\n", fileCount);
  
  // Sorteer bestanden op naam
  for (int i = 0; i < fileCount - 1; i++) {
    for (int j = 0; j < fileCount - i - 1; j++) {
      if (files[j] > files[j + 1]) {
        String temp = files[j];
        files[j] = files[j + 1];
        files[j + 1] = temp;
      }
    }
  }
}

static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  int16_t tw = strlen(txt) * 6;
  int16_t th = 8;
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawDoubleButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt1, const char* txt2, uint16_t color) {
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  int16_t tw1 = strlen(txt1) * 6;
  int16_t textX1 = x + 4 + (w - 8 - tw1) / 2 - 4 - 4 + 4;
  int16_t textY1 = y + h/2 - 8 + 5;
  g->setCursor(textX1, textY1);
  g->print(txt1);
  
  int16_t tw2 = strlen(txt2) * 6;
  int16_t textX2 = x + 4 + (w - 8 - tw2) / 2 - 4 - 2;
  int16_t textY2 = y + h/2 + 4 + 2 + 3;
  g->setCursor(textX2, textY2);
  g->print(txt2);
}

static void drawFileList() {
  g->fillRect(8, fileListY-2, SCR_W-100, 122, COL_BG);
  
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  
  if (fileCount == 0) {
    g->setCursor(10, fileListY + 10);
    g->print("Geen opnames gevonden");
    return;
  }
  
  for (int i = 0; i < fileCount; i++) {
    int16_t y = fileListY + i * fileItemH;
    
    if (i == selectedFile) {
      g->setTextColor(0xF800);  // Rood voor geselecteerd
    } else {
      g->setTextColor(COL_TX);
    }
    
    g->setCursor(15, y + 6);
    g->print(files[i]);
    
    // FIXED: Gebruik SD_MMC voor file size check
    String fullPath = "/" + files[i];
    if (SD_MMC.exists(fullPath.c_str())) {
      //File f = SD_MMC.open(fullPath.c_str());
      fs::File f = SD_MMC.open(fullPath.c_str());
      if (f) {
        long size = f.size();
        f.close();
        g->setCursor(SCR_W / 2 - 35, y + 6);
        g->print(String(size/1024) + "KB");
      }
    }
  }
}

void playlist_begin(TFT_eSPI* gfx) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t titleW = strlen("Opnames") * 12;
  int16_t availableW = SCR_W - 100;
  g->setCursor((availableW - titleW) / 2, 25);
  g->print("Opnames");
  
  // FIXED: Scan met SD_MMC
  scanSDFiles();
  
  // Knoppen layout
  int16_t btnW = 90;
  int16_t btnH = (SCR_H - 10) / 4;
  int16_t btnX = SCR_W - btnW - 2;
  int16_t startY = 5;
  
  bxPlayX = btnX; bxPlayY = startY; bxPlayW = btnW; bxPlayH = btnH;
  bxDelX = btnX; bxDelY = startY + btnH; bxDelW = btnW; bxDelH = btnH;
  bxAiX = btnX; bxAiY = startY + (btnH * 2); bxAiW = btnW; bxAiH = btnH;
  bxBackX = btnX; bxBackY = startY + (btnH * 3); bxBackW = btnW; bxBackH = btnH;
  
  drawButton(bxPlayX, bxPlayY, bxPlayW, bxPlayH, "PLAY", COL_BTN_PLAY);
  drawButton(bxDelX, bxDelY, bxDelW, bxDelH, "DELETE", COL_BTN_DELETE);
  drawDoubleButton(bxAiX, bxAiY, bxAiW, bxAiH, "AI", "Analyze", COL_BTN_AI);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
  
  drawFileList();
}

PlaylistEvent playlist_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return PE_NONE;
  
  uint32_t now = millis();
  if (now - lastTouchMs < PLAYLIST_COOLDOWN_MS) return PE_NONE;
  
  if (inRect(x, y, bxPlayX, bxPlayY, bxPlayW, bxPlayH)) {
    lastTouchMs = now;
    return (fileCount > 0) ? PE_PLAY_FILE : PE_NONE;
  }
  if (inRect(x, y, bxDelX, bxDelY, bxDelW, bxDelH)) {
    lastTouchMs = now;
    return (fileCount > 0) ? PE_DELETE_CONFIRM : PE_NONE;
  }
  if (inRect(x, y, bxAiX, bxAiY, bxAiW, bxAiH)) {
    lastTouchMs = now;
    return (fileCount > 0) ? PE_AI_ANALYZE : PE_NONE;
  }
  if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
    lastTouchMs = now;
    return PE_BACK;
  }
  
  if (inRect(x, y, 10, fileListY, SCR_W-106, fileCount * fileItemH)) {
    int newSelected = (y - fileListY) / fileItemH;
    if (newSelected >= 0 && newSelected < fileCount && newSelected != selectedFile) {
      lastTouchMs = now;
      selectedFile = newSelected;
      drawFileList();
    }
  }
  
  return PE_NONE;
}

String playlist_getSelectedFile() {
  if (selectedFile >= 0 && selectedFile < fileCount) {
    return files[selectedFile];
  }
  return "";
}