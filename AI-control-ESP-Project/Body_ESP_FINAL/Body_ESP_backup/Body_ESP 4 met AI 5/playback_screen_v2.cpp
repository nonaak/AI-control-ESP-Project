/*
  PLAYBACK SCREEN V2 - Implementatie
  
  Herontworpen playback scherm met:
  - Groot level indicator links
  - Grafieken rechts
  - Afspeelbalk met annotatie markers
  - Duidelijke AI vs User vergelijking
*/

#include "playback_screen_v2.h"

// Global instance
PlaybackScreenV2 playbackScreen;

// ═══════════════════════════════════════════════════════════════════════════
//                         CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

PlaybackScreenV2::PlaybackScreenV2() {
  gfx = nullptr;
  filename[0] = '\0';
  currentTime = 0;
  totalTime = 0;
  speed = 100.0f;
  isPaused = false;
  
  currentLevel = 0;
  aiPredictedLevel = -1;
  userAnnotatedLevel = -1;
  
  hr = 0;
  temp = 0;
  gsr = 0;
  
  markerCount = 0;
  levelHistoryIdx = 0;
  levelHistoryFull = false;
  
  selectedButtonIdx = 1;  // Default: PLAY/PAUZE
  staticDrawn = false;
  
  memset(levelHistory, 0, sizeof(levelHistory));
}

// ═══════════════════════════════════════════════════════════════════════════
//                         SETUP
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::begin(Arduino_GFX* graphics) {
  gfx = graphics;
  staticDrawn = false;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         STATE UPDATES
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::setFilename(const char* fname) {
  strncpy(filename, fname, 63);
  filename[63] = '\0';
}

void PlaybackScreenV2::setProgress(float current, float total) {
  currentTime = current;
  totalTime = total;
}

void PlaybackScreenV2::setSpeed(float speedPercent) {
  speed = speedPercent;
}

void PlaybackScreenV2::setPaused(bool paused) {
  isPaused = paused;
}

void PlaybackScreenV2::setCurrentLevel(int level) {
  currentLevel = constrain(level, 0, 7);
}

void PlaybackScreenV2::setAIPrediction(int aiLevel) {
  aiPredictedLevel = aiLevel;
}

void PlaybackScreenV2::setUserAnnotation(int userLevel) {
  userAnnotatedLevel = userLevel;
}

void PlaybackScreenV2::setSensorValues(float heartRate, float temperature, float gsrVal) {
  hr = heartRate;
  temp = temperature;
  gsr = gsrVal;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         MARKERS
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::clearMarkers() {
  markerCount = 0;
}

void PlaybackScreenV2::addMarker(float timestamp, int level, bool isAI, bool isEdge) {
  if (markerCount >= MAX_VISIBLE_MARKERS) return;
  
  markers[markerCount].timestamp = timestamp;
  markers[markerCount].level = level;
  markers[markerCount].isAI = isAI;
  markers[markerCount].isEdge = isEdge;
  markerCount++;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         LEVEL HISTORY
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::pushLevelSample(int level) {
  levelHistory[levelHistoryIdx] = (int8_t)constrain(level, 0, 7);
  levelHistoryIdx = (levelHistoryIdx + 1) % 100;
  if (levelHistoryIdx == 0) levelHistoryFull = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         HELPERS
// ═══════════════════════════════════════════════════════════════════════════

uint16_t PlaybackScreenV2::getLevelColor(int level) {
  if (level < 0 || level > 7) return 0x7BEF;  // Grijs voor onbekend
  return STRESS_COLORS[level];
}

void PlaybackScreenV2::drawCenteredText(const char* text, int x, int y, int w, uint16_t color, uint16_t bg) {
  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
  gfx->setTextColor(color, bg);
  gfx->setCursor(x + (w - tw) / 2 - x1, y);
  gfx->print(text);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - STATIC ELEMENTS
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawStaticElements() {
  if (!gfx) return;
  
  // Wis scherm
  gfx->fillScreen(0x0000);  // Zwart
  
  // Titel balk achtergrond (bovenaan)
  gfx->fillRect(0, 0, PB_SCREEN_W, PB_TITLE_H, 0x2104);
  gfx->drawLine(0, PB_TITLE_H - 1, PB_SCREEN_W, PB_TITLE_H - 1, 0x4A49);
  
  // Level paneel kader (links)
  gfx->drawRect(PB_LEVEL_PANEL_X + 2, PB_LEVEL_PANEL_Y + 2, 
                PB_LEVEL_PANEL_W - 4, PB_LEVEL_PANEL_H - 4, 0x4A49);
  
  // Scheiding tussen level paneel en grafieken
  gfx->drawLine(PB_LEVEL_PANEL_W, PB_LEVEL_PANEL_Y, 
                PB_LEVEL_PANEL_W, PB_LEVEL_PANEL_Y + PB_LEVEL_PANEL_H, 0x4A49);
  
  // Grafieken area kader (rechts)
  gfx->drawRect(PB_GRAPH_X - 3, PB_GRAPH_Y + 2, 
                PB_GRAPH_W + 1, PB_GRAPH_H - 4, 0x4A49);
  
  // Progress bar kader
  gfx->drawRect(PB_PROGRESS_X - 1, PB_PROGRESS_Y - 1, 
                PB_PROGRESS_W + 2, PB_PROGRESS_H + 2, 0x4A49);
  
  staticDrawn = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - TITLE BAR
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawTitleBar() {
  if (!gfx) return;
  
  // Wis titel area
  gfx->fillRect(0, 0, PB_SCREEN_W, PB_TITLE_H, 0x2104);
  gfx->drawLine(0, PB_TITLE_H - 1, PB_SCREEN_W, PB_TITLE_H - 1, 0x4A49);
  
  gfx->setTextSize(1);
  gfx->setTextColor(0xFFFF, 0x2104);
  
  // Play/Pause icoon links
  int iconX = 5;
  int iconY = 8;
  if (isPaused) {
    // Pauze icoon (twee balkjes) - geel
    gfx->fillRect(iconX, iconY, 4, 12, 0xFFE0);
    gfx->fillRect(iconX + 6, iconY, 4, 12, 0xFFE0);
  } else {
    // Play icoon (driehoek) - groen
    gfx->fillTriangle(iconX, iconY, iconX, iconY + 12, iconX + 10, iconY + 6, 0x07E0);
  }
  
  // Bestandsnaam (na icoon)
  gfx->setCursor(20, 10);
  if (strlen(filename) > 0) {
    // Kort af indien nodig
    char shortName[24];
    if (strlen(filename) > 20) {
      strncpy(shortName, filename, 17);
      shortName[17] = '.';
      shortName[18] = '.';
      shortName[19] = '.';
      shortName[20] = '\0';
    } else {
      strcpy(shortName, filename);
    }
    gfx->print(shortName);
  } else {
    gfx->print("(geen bestand)");
  }
  
  // Tijd rechts
  char timeStr[20];
  int curMin = (int)(currentTime / 60);
  int curSec = (int)currentTime % 60;
  int totMin = (int)(totalTime / 60);
  int totSec = (int)totalTime % 60;
  sprintf(timeStr, "%d:%02d / %d:%02d", curMin, curSec, totMin, totSec);
  
  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor(PB_SCREEN_W - tw - 5, 10);
  gfx->print(timeStr);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - LEVEL PANEL
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawLevelPanel() {
  if (!gfx) return;
  
  int panelX = PB_LEVEL_PANEL_X + 5;
  int panelY = PB_LEVEL_PANEL_Y + 5;
  int panelW = PB_LEVEL_PANEL_W - 10;
  int panelH = PB_LEVEL_PANEL_H - 10;
  
  uint16_t levelColor = getLevelColor(currentLevel);
  uint16_t bgColor = 0x0000;
  
  // Wis panel
  gfx->fillRect(panelX, panelY, panelW, panelH, bgColor);
  
  // ─── Groot Level Nummer Box ───
  int boxX = panelX + 8;
  int boxY = panelY + 5;
  int boxW = panelW - 16;
  int boxH = 75;
  
  // Gekleurde box
  gfx->fillRoundRect(boxX, boxY, boxW, boxH, 8, levelColor);
  gfx->drawRoundRect(boxX, boxY, boxW, boxH, 8, 0xFFFF);
  
  // Level nummer (groot, gecentreerd)
  char levelNum[4];
  sprintf(levelNum, "%d", currentLevel);
  
  gfx->setTextSize(5);  // Groter!
  
  uint16_t textColor = (currentLevel < 3) ? 0x0000 : 0xFFFF;
  gfx->setTextColor(textColor, levelColor);
  
  // Centreer: bij size 5 is een cijfer ~30px breed, ~35px hoog
  int textX = boxX + (boxW / 2) - 15;
  int textY = boxY + (boxH / 2) - 18;
  gfx->setCursor(textX, textY);
  gfx->print(levelNum);
  
  gfx->setTextSize(1);
  
  // ─── Level Label (onder box) ───
  int16_t x1, y1;
  uint16_t tw, th;
  int labelY = boxY + boxH + 8;
  gfx->setTextColor(levelColor, bgColor);
  gfx->getTextBounds(STRESS_LABELS_SHORT[currentLevel], 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor(panelX + (panelW - tw) / 2 - x1, labelY);
  gfx->print(STRESS_LABELS_SHORT[currentLevel]);
  
  // ─── AI vs Jij vergelijking ───
  int compareY = labelY + 20;
  
  // Horizontale lijn
  gfx->drawLine(panelX + 5, compareY - 3, panelX + panelW - 5, compareY - 3, 0x4A49);
  
  if (aiPredictedLevel >= 0) {
    // AI: X → Jij: Y
    gfx->setTextColor(0x07FF, bgColor);  // Cyan
    gfx->setCursor(panelX + 5, compareY + 5);
    gfx->printf("AI: %d", aiPredictedLevel);
    
    if (userAnnotatedLevel >= 0) {
      gfx->setTextColor(0xFFFF, bgColor);
      gfx->print(" -> ");
      gfx->setTextColor(0x07E0, bgColor);  // Groen
      gfx->printf("Jij: %d", userAnnotatedLevel);
      
      // Correctie
      int correction = userAnnotatedLevel - aiPredictedLevel;
      uint16_t corrColor = (correction > 0) ? 0xF800 : 
                           (correction < 0) ? 0x07E0 : 0x7BEF;
      gfx->setTextColor(corrColor, bgColor);
      gfx->setCursor(panelX + 5, compareY + 20);
      gfx->printf("Correctie: %+d", correction);
    }
  } else {
    gfx->setTextColor(0x7BEF, bgColor);
    gfx->setCursor(panelX + 5, compareY + 5);
    gfx->print("Geen AI data");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - PROGRESS BAR
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawProgressBar() {
  if (!gfx) return;
  
  // Wis progress bar area
  gfx->fillRect(PB_PROGRESS_X, PB_PROGRESS_Y, PB_PROGRESS_W, PB_PROGRESS_H, 0x2104);
  
  // Progress bar achtergrond
  int barX = PB_PROGRESS_X + 2;
  int barY = PB_PROGRESS_Y + 2;
  int barW = PB_PROGRESS_W - 4;
  int barH = PB_PROGRESS_H - 4;
  
  gfx->fillRect(barX, barY, barW, barH, 0x4208);  // Donkergrijs
  
  // Progress fill
  float progress = (totalTime > 0) ? (currentTime / totalTime) : 0;
  int fillW = (int)(barW * progress);
  if (fillW > 0) {
    // Gradient van groen naar huidige level kleur
    uint16_t fillColor = getLevelColor(currentLevel);
    gfx->fillRect(barX, barY, fillW, barH, fillColor);
  }
  
  // ─── Teken Markers ───
  for (int i = 0; i < markerCount; i++) {
    PlaybackMarker& m = markers[i];
    if (totalTime <= 0) continue;
    
    int markerX = barX + (int)((m.timestamp / totalTime) * barW);
    markerX = constrain(markerX, barX, barX + barW - 2);
    
    uint16_t markerColor;
    int markerH = barH - 4;
    int markerY = barY + 2;
    
    if (m.isEdge) {
      // Edge marker: wit driehoekje bovenaan
      gfx->fillTriangle(markerX, markerY, markerX - 3, markerY - 5, markerX + 3, markerY - 5, 0xFFFF);
    } else if (m.isAI) {
      // AI marker: dunne lijn in level kleur
      gfx->drawLine(markerX, markerY, markerX, markerY + markerH, getLevelColor(m.level));
    } else {
      // User annotatie: dikkere lijn in groen
      gfx->fillRect(markerX - 1, markerY, 3, markerH, 0x07E0);
      // Klein driehoekje onderaan
      gfx->fillTriangle(markerX, markerY + markerH, 
                        markerX - 4, markerY + markerH + 6, 
                        markerX + 4, markerY + markerH + 6, 0x07E0);
    }
  }
  
  // ─── Huidige Positie Indicator ───
  int indicatorX = barX + fillW;
  gfx->fillRect(indicatorX - 1, barY - 3, 3, barH + 6, 0xFFFF);
  gfx->fillTriangle(indicatorX, barY - 3, indicatorX - 4, barY - 8, indicatorX + 4, barY - 8, 0xFFFF);
  
  // ─── Speed Controls Rechts ───
  int speedX = PB_SPEED_X;
  int speedY = PB_PROGRESS_Y;
  int btnW = 28;
  int btnH = PB_PROGRESS_H;
  
  // - knop (blauw)
  gfx->fillRoundRect(speedX, speedY, btnW, btnH, 4, 0x001F);
  gfx->drawRoundRect(speedX, speedY, btnW, btnH, 4, 0xFFFF);
  gfx->setTextSize(1);
  gfx->setTextColor(0xFFFF, 0x001F);
  gfx->setCursor(speedX + 11, speedY + 10);  // Gecentreerd
  gfx->print("-");
  
  // Speed percentage
  int textX = speedX + btnW + 3;
  int textW = 40;
  gfx->fillRect(textX, speedY, textW, btnH, 0x0000);
  gfx->setTextColor(0xFFFF, 0x0000);
  char speedStr[8];
  sprintf(speedStr, "%.0f%%", speed);
  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(speedStr, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor(textX + (textW - tw) / 2, speedY + 10);
  gfx->print(speedStr);
  
  // + knop (rood)
  int plusX = textX + textW + 3;
  gfx->fillRoundRect(plusX, speedY, btnW, btnH, 4, 0xF800);
  gfx->drawRoundRect(plusX, speedY, btnW, btnH, 4, 0xFFFF);
  gfx->setTextColor(0xFFFF, 0xF800);
  gfx->setCursor(plusX + 10, speedY + 10);  // Gecentreerd
  gfx->print("+");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - SENSOR GRAPHS (rechts)
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawSensorGraphs() {
  if (!gfx) return;
  
  int graphX = PB_GRAPH_X;
  int graphW = PB_GRAPH_W - 10;
  int labelW = 35;  // Ruimte voor label
  int barX = graphX + labelW;
  int barW = graphW - labelW - 5;
  int rowH = 28;
  int spacing = 5;
  
  // ─── HR Grafiek ───
  int hrY = PB_GRAPH_Y + 8;
  gfx->fillRect(barX, hrY, barW, rowH - 2, 0x1082);  // Achtergrond
  gfx->setTextColor(0xF800, 0x0000);  // Rood
  gfx->setCursor(graphX, hrY + 9);
  gfx->print("HR");
  // Waarde in midden van balk
  char hrStr[16];
  sprintf(hrStr, "%.0f BPM", hr);
  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(hrStr, 0, 0, &x1, &y1, &tw, &th);
  gfx->setTextColor(0xF800, 0x1082);
  gfx->setCursor(barX + (barW - tw) / 2, hrY + 9);
  gfx->print(hrStr);
  
  // ─── TEMP Grafiek ───
  int tempY = hrY + rowH + spacing;
  gfx->fillRect(barX, tempY, barW, rowH - 2, 0x1082);
  gfx->setTextColor(0xFD20, 0x0000);  // Oranje
  gfx->setCursor(graphX, tempY + 9);
  gfx->print("TEMP");
  char tempStr[16];
  sprintf(tempStr, "%.1f C", temp);
  gfx->getTextBounds(tempStr, 0, 0, &x1, &y1, &tw, &th);
  gfx->setTextColor(0xFD20, 0x1082);
  gfx->setCursor(barX + (barW - tw) / 2, tempY + 9);
  gfx->print(tempStr);
  
  // ─── GSR Grafiek ───
  int gsrY = tempY + rowH + spacing;
  gfx->fillRect(barX, gsrY, barW, rowH - 2, 0x1082);
  gfx->setTextColor(0xFFE0, 0x0000);  // Geel
  gfx->setCursor(graphX, gsrY + 9);
  gfx->print("GSR");
  char gsrStr[16];
  sprintf(gsrStr, "%.0f", gsr);
  gfx->getTextBounds(gsrStr, 0, 0, &x1, &y1, &tw, &th);
  gfx->setTextColor(0xFFE0, 0x1082);
  gfx->setCursor(barX + (barW - tw) / 2, gsrY + 9);
  gfx->print(gsrStr);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - LEVEL GRAPH
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawLevelGraph() {
  if (!gfx) return;
  
  int graphX = PB_GRAPH_X;
  int graphW = PB_GRAPH_W - 10;
  int labelW = 40;
  int barX = graphX + labelW;
  int barW = graphW - labelW - 20;  // Ruimte voor 7/0 rechts
  int graphY = PB_GRAPH_Y + 110;  // Onder de sensor grafieken
  int graphH = 35;
  
  // Achtergrond
  gfx->fillRect(barX, graphY, barW, graphH, 0x1082);
  
  // Label links, verticaal gecentreerd
  gfx->setTextColor(0xF81F, 0x0000);  // Magenta
  gfx->setCursor(graphX, graphY + (graphH / 2) - 4);
  gfx->print("LEVEL");
  
  // Y-as labels rechts van grafiek
  gfx->setTextColor(0x7BEF, 0x0000);
  gfx->setCursor(barX + barW + 5, graphY + 2);
  gfx->print("7");
  gfx->setCursor(barX + barW + 5, graphY + graphH - 10);
  gfx->print("0");
  
  // Teken level lijn
  int count = levelHistoryFull ? 100 : levelHistoryIdx;
  if (count > 1) {
    int startIdx = levelHistoryFull ? levelHistoryIdx : 0;
    float step = (float)barW / 100.0f;
    
    for (int i = 1; i < count; i++) {
      int idx1 = (startIdx + i - 1) % 100;
      int idx2 = (startIdx + i) % 100;
      
      int x1 = barX + (int)((i - 1) * step);
      int x2 = barX + (int)(i * step);
      
      int y1 = graphY + graphH - 3 - (levelHistory[idx1] * (graphH - 6) / 7);
      int y2 = graphY + graphH - 3 - (levelHistory[idx2] * (graphH - 6) / 7);
      
      uint16_t color = getLevelColor(levelHistory[idx2]);
      gfx->drawLine(x1, y1, x2, y2, color);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - BUTTONS
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawButtons(int selectedIdx) {
  if (!gfx) return;
  
  selectedButtonIdx = selectedIdx;
  
  // LANDSCAPE: Knoppen HORIZONTAAL onderaan
  int y = PB_BUTTON_Y;
  int h = PB_BUTTON_H;
  int w = PB_BUTTON_W;
  int spacing = PB_BUTTON_SPACING;
  
  // Bereken start X (gecentreerd)
  int totalW = 5 * w + 4 * spacing;
  int startX = (PB_SCREEN_W - totalW) / 2;
  
  struct ButtonDef {
    const char* label;
    uint16_t color;
  };
  
  ButtonDef buttons[5] = {
    {"STOP",     0xFD20},   // Oranje
    {isPaused ? "PLAY" : "PAUZE", isPaused ? 0x07E0 : 0xF800},  // Groen/Rood
    {"-10s",     0x001F},   // Blauw
    {"+10s",     0x001F},   // Blauw
    {"GEVOEL",   0xF81F}    // Magenta
  };
  
  for (int i = 0; i < 5; i++) {
    int x = startX + i * (w + spacing);
    
    // Knop achtergrond
    gfx->fillRoundRect(x, y, w, h, 6, buttons[i].color);
    
    // Selectie highlight
    if (i == selectedIdx) {
      gfx->drawRoundRect(x - 2, y - 2, w + 4, h + 4, 8, 0xFFE0);  // Geel
      gfx->drawRoundRect(x - 1, y - 1, w + 2, h + 2, 7, 0xFFE0);
    }
    
    // Witte rand
    gfx->drawRoundRect(x, y, w, h, 6, 0xFFFF);
    
    // Label - gecentreerd
    uint16_t textColor = (buttons[i].color == 0xFFE0 || buttons[i].color == 0x07E0) ? 0x0000 : 0xFFFF;
    gfx->setTextColor(textColor, buttons[i].color);
    
    int16_t x1, y1;
    uint16_t tw, th;
    gfx->getTextBounds(buttons[i].label, 0, 0, &x1, &y1, &tw, &th);
    gfx->setCursor(x + (w - tw) / 2 - x1, y + (h + th) / 2);
    gfx->print(buttons[i].label);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DRAWING - DYNAMIC ELEMENTS
// ═══════════════════════════════════════════════════════════════════════════

void PlaybackScreenV2::drawDynamicElements() {
  if (!gfx) return;
  
  if (!staticDrawn) {
    drawStaticElements();
  }
  
  drawTitleBar();
  drawLevelPanel();
  drawSensorGraphs();  // 🔥 NIEUW: HR, TEMP, GSR rechts
  drawProgressBar();
  drawLevelGraph();
  drawButtons(selectedButtonIdx);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         TOUCH HANDLING
// ═══════════════════════════════════════════════════════════════════════════

int PlaybackScreenV2::handleTouch(int x, int y) {
  // Check buttons
  if (y >= PB_BUTTON_Y && y <= PB_BUTTON_Y + PB_BUTTON_H) {
    int w = PB_BUTTON_W;
    int spacing = PB_BUTTON_SPACING;
    int totalW = 5 * w + 4 * spacing;
    int startX = (PB_SCREEN_W - totalW) / 2;
    
    for (int i = 0; i < 5; i++) {
      int btnX = startX + i * (w + spacing);
      if (x >= btnX && x <= btnX + w) {
        return i;  // Return button index
      }
    }
  }
  
  // Check speed controls
  if (y >= PB_PROGRESS_Y && y <= PB_PROGRESS_Y + PB_PROGRESS_H) {
    int speedX = PB_SPEED_X;
    int btnW = 25;
    
    // - knop
    if (x >= speedX && x <= speedX + btnW) {
      return 10;  // Special: speed down
    }
    
    // + knop
    int plusX = speedX + btnW + 3 + 45 + 3;
    if (x >= plusX && x <= plusX + btnW) {
      return 11;  // Special: speed up
    }
    
    // Progress bar click = seek
    if (x >= PB_PROGRESS_X && x <= PB_PROGRESS_X + PB_PROGRESS_W) {
      return 20;  // Special: seek (use x position for calculation)
    }
  }
  
  return -1;  // No hit
}

void PlaybackScreenV2::setSelectedButton(int idx) {
  selectedButtonIdx = constrain(idx, 0, 4);
}
