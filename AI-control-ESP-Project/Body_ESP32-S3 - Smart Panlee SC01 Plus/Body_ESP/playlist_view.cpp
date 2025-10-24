#include "playlist_view.h"
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include "input_touch.h"

static Arduino_GFX* g = nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_PLAY=0x07E0;    // Groen voor afspelen
static const uint16_t COL_BTN_DELETE=0xF800;  // Rood voor verwijderen
static const uint16_t COL_BTN_AI=0xF81F;      // COL_FRAME2 kleur voor AI (andere kader kleur)
static const uint16_t COL_BTN_BACK=0x001F;    // Blauw voor terug
static const uint16_t COL_FILE_BG=0x2104;     // Donkergrijs voor bestanden
static const uint16_t COL_FILE_SEL=0x4A69;    // Lichter grijs voor geselecteerd

static int16_t SCR_W=320, SCR_H=240;
static String files[10];  // Max 10 bestanden tonen
static int fileCount = 0;
static int selectedFile = 0;

// Touch cooldown voor playlist
static uint32_t lastTouchMs = 0;
static const uint16_t PLAYLIST_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles
static int16_t bxPlayX,bxPlayY,bxPlayW,bxPlayH;
static int16_t bxDelX,bxDelY,bxDelW,bxDelH;
static int16_t bxAiX,bxAiY,bxAiW,bxAiH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

// File list area
static int16_t fileListY = 60;
static int16_t fileItemH = 20;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static void scanSDFiles() {
  fileCount = 0;
  selectedFile = 0;
  
  if (!SD.begin(5)) return;  // SD CS pin 5
  
  File root = SD.open("/");
  if (!root) return;
  
  File file = root.openNextFile();
  while (file && fileCount < 10) {
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.startsWith("data") && name.endsWith(".csv")) {
        files[fileCount] = name;
        fileCount++;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
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
  // HoofdESP style button met versiering terug
  g->fillRoundRect(x, y, w, h, 8, COL_BG);  // Background
  g->drawRoundRect(x, y, w, h, 8, COL_FR);  // Outer border
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);  // Inner border
  
  // Fill button with color
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  // Button text with better centering
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Better text centering calculation
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;  // Better vertical centering
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawDoubleButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt1, const char* txt2, uint16_t color) {
  // HoofdESP style button met versiering terug
  g->fillRoundRect(x, y, w, h, 8, COL_BG);  // Background
  g->drawRoundRect(x, y, w, h, 8, COL_FR);  // Outer border
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);  // Inner border
  
  // Fill button with color
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  // Button text - twee regels
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Eerste regel "AI" - 4px naar rechts van vorige positie
  int16_t tw1 = strlen(txt1) * 6;
  int16_t textX1 = x + 4 + (w - 8 - tw1) / 2 - 4 - 4 + 4;  // 4px naar rechts (totaal 4px naar links)
  int16_t textY1 = y + h/2 - 8 + 5;  // 5px naar beneden (blijft)
  g->setCursor(textX1, textY1);
  g->print(txt1);
  
  // Tweede regel "Analyze" - 2px naar links en 2px naar beneden
  int16_t tw2 = strlen(txt2) * 6;
  int16_t textX2 = x + 4 + (w - 8 - tw2) / 2 - 4 - 2;  // 2px meer naar links
  int16_t textY2 = y + h/2 + 4 + 2 + 3;  // 2px + 3px = 5px naar beneden
  g->setCursor(textX2, textY2);
  g->print(txt2);
}

static void drawFileList() {
  // Wis file list gebied - meer ruimte laten voor knoppen rechts
  g->fillRect(8, fileListY-2, SCR_W-100, 122, COL_BG);  // Nog meer ruimte voor knoppen
  
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  
  if (fileCount == 0) {
    g->setCursor(10, fileListY + 10);
    g->print("Geen opnames gevonden");
    return;
  }
  
  // Toon bestanden
  for (int i = 0; i < fileCount; i++) {
    int16_t y = fileListY + i * fileItemH;
    
    // Geen achtergrond highlight meer - alleen tekst kleur wijzigen
    
    // Tekst kleur: rood voor geselecteerd bestand, wit voor andere
    if (i == selectedFile) {
      g->setTextColor(0xF800);  // Rood voor geselecteerd bestand
    } else {
      g->setTextColor(COL_TX);  // Wit voor niet-geselecteerde bestanden
    }
    
    g->setCursor(15, y + 6);
    g->print(files[i]);
    
    // Toon bestandsgrootte als beschikbaar
    if (SD.exists("/" + files[i])) {
      File f = SD.open("/" + files[i]);
      if (f) {
        long size = f.size();
        f.close();
        // Plaats KB tekst meer naar links om overlappen met knoppen te voorkomen
        g->setCursor(SCR_W / 2 - 35, y + 6);  // Verder naar links
        g->print(String(size/1024) + "KB");
      }
    }
  }
}

void playlist_begin(Arduino_GFX* gfx) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  // Center title more in available space (left of buttons)
  int16_t titleW = strlen("Opnames") * 12;  // Width for text size 2
  int16_t availableW = SCR_W - 100;  // Space left of wider buttons
  g->setCursor((availableW - titleW) / 2, 25);  // Lower Y position so it's visible
  g->print("Opnames");
  
  // Scan SD card voor bestanden
  scanSDFiles();
  
  // Knoppen layout - rechts in 4 gelijke delen, nu alle 4 gebruikt
  int16_t btnW = 90;  // Much wider buttons so text fits properly
  int16_t btnH = (SCR_H - 10) / 4;  // Height = screen height / 4
  int16_t btnX = SCR_W - btnW - 2;  // Right side with smaller margin
  int16_t startY = 5;
  
  // 4 knoppen: PLAY, DELETE, AI, TERUG
  bxPlayX = btnX; bxPlayY = startY; bxPlayW = btnW; bxPlayH = btnH;
  bxDelX = btnX; bxDelY = startY + btnH; bxDelW = btnW; bxDelH = btnH;
  bxAiX = btnX; bxAiY = startY + (btnH * 2); bxAiW = btnW; bxAiH = btnH;
  bxBackX = btnX; bxBackY = startY + (btnH * 3); bxBackW = btnW; bxBackH = btnH;
  
  // Teken knoppen
  drawButton(bxPlayX, bxPlayY, bxPlayW, bxPlayH, "PLAY", COL_BTN_PLAY);
  drawButton(bxDelX, bxDelY, bxDelW, bxDelH, "DELETE", COL_BTN_DELETE);
  drawDoubleButton(bxAiX, bxAiY, bxAiW, bxAiH, "AI", "Analyze", COL_BTN_AI);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
  
  // Teken bestandenlijst
  drawFileList();
}

PlaylistEvent playlist_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return PE_NONE;
  
  // Check cooldown
  uint32_t now = millis();
  if (now - lastTouchMs < PLAYLIST_COOLDOWN_MS) return PE_NONE;
  
  // Check button taps
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
  
  // Check file selection - laat ruimte voor bredere knoppen rechts
  if (inRect(x, y, 10, fileListY, SCR_W-106, fileCount * fileItemH)) {
    int newSelected = (y - fileListY) / fileItemH;
    if (newSelected >= 0 && newSelected < fileCount && newSelected != selectedFile) {
      lastTouchMs = now;
      selectedFile = newSelected;
      drawFileList();  // Refresh lijst met nieuwe selectie
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