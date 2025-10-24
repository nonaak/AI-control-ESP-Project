#include "ai_analyze_view.h"
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include "input_touch.h"
#include "ai_overrule.h"
#include "ai_event_config_view.h"
#include "ai_training_view.h"

// ===== AI GEBEURTENIS DEFINITIES =====
// Deze definities kunnen later aangepast worden voor specifieke gebeurtenissen
static const char* eventNames[] = {
  "Gebeurtenis 1: Hoge hartslag gedetecteerd",
  "Gebeurtenis 2: Temperatuur boven threshold", 
  "Gebeurtenis 3: GSR stress indicator",
  "Gebeurtenis 4: Lage hartslag waarschuwing",
  "Gebeurtenis 5: Machine snelheidspieken",
  "Gebeurtenis 6: Combinatie stress signalen",
  "Gebeurtenis 7: Onregelmatige hartslag",
  "Gebeurtenis 8: Langdurige stress periode"
};

static const uint16_t eventColors[] = {
  0xF800,  // Rood - Hoge hartslag
  0xFD20,  // Oranje - Hoge temperatuur  
  0xFFE0,  // Geel - GSR stress
  0x001F,  // Blauw - Lage hartslag
  0xF81F,  // Magenta - Machine pieken
  0x780F,  // Paars - Combinatie stress
  0x07FF,  // Cyaan - Onregelmatige hartslag
  0xF800   // Donkerrood - Langdurige stress
};

#define MAX_EVENT_TYPES 8
// ===== EINDE GEBEURTENIS DEFINITIES =====

static Arduino_GFX* g = nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_SELECT=0x07E0;    // Groen voor bestand selecteren
static const uint16_t COL_BTN_ANALYZE=0xFFE0;   // Geel voor analyse
static const uint16_t COL_BTN_BACK=0x051F;      // Helder blauw voor terug (was 0x001F)
static const uint16_t COL_BTN_NEXT=0xF81F;      // Magenta voor volgende
static const uint16_t COL_BTN_PREV=0xFDA0;      // Helder oranje voor vorige (was 0x780F)
static const uint16_t COL_RESULT_BG=0x2104;     // Donkergrijs voor resultaten

static int16_t SCR_W=320, SCR_H=240;
static uint32_t lastTouchMs = 0;
static const uint16_t ANALYZE_COOLDOWN_MS = 800;  // Consistente cooldown

// Button areas
static int16_t btnSelectX, btnSelectY, btnAnalyzeX, btnAnalyzeY, btnBackX, btnBackY;
static int16_t btnNextX, btnNextY, btnPrevX, btnPrevY;
static const int16_t BTN_W = 110, BTN_H = 42;  // Larger buttons for stylus use

// Analyse state
static AnalysisResult currentResult;
static bool analysisComplete = false;
static uint8_t currentPage = 0;  // 0=file select, 1=results, 2=timeline
static String selectedFile = "";
static int selectedFileIndex = 0;
static String availableFiles[10];  // Max 10 files
static int fileCount = 0;

// Timeline scroll state
static float timelineScroll = 0.0f;  // 0.0 = begin, 1.0 = eind
static bool timelineScrolling = false;
static int16_t timelineStartX = 0;
static uint32_t lastTimelineUpdate = 0;  // Anti-flicker

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static void addEvent(uint32_t timeMs, uint8_t eventType, float severity) {
  if (currentResult.eventCount >= 50) return;  // Max events bereikt
  
  EventData& event = currentResult.events[currentResult.eventCount];
  event.timeMs = timeMs;
  event.eventType = eventType;
  event.severity = severity;
  currentResult.eventCount++;
}

static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  // Button met afgeronde hoeken zoals andere menus
  g->fillRoundRect(x, y, w, h, 8, COL_BG);  // Background
  g->drawRoundRect(x, y, w, h, 8, COL_FR);  // Outer border
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);  // Inner border
  
  // Fill button with color
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  // Button text with better centering - simple readable font
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

static void drawFileSelection() {
  Serial.println("[AI] drawFileSelection() starting...");
  
  // Safety check for graphics pointer
  if (g == nullptr) {
    Serial.println("[AI] ERROR: Graphics pointer is NULL!");
    return;
  }
  
  Serial.println("[AI] Filling screen...");
  g->fillScreen(COL_BG);
  
  // Debug output
  Serial.printf("[AI] selectedFile: '%s', length: %d\n", 
                selectedFile.c_str(), selectedFile.length());
  
  // Title - smaller en better positioned
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("AI Data Analyse", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 15);  // Centered
  g->print("AI Data Analyse");
  
  // Instructions
  g->setTextSize(1);
  g->setCursor(10, 35);
  g->print("Selecteer CSV bestand voor analyse:");
  
  // Selected file
  g->setCursor(10, 50);
  if (selectedFile.length() > 0) {
    g->setTextColor(0x07E0);  // Groen
    g->printf("Geselecteerd: %s", selectedFile.c_str());
  } else {
    g->setTextColor(0xF800);  // Rood
    g->print("Geen bestand geselecteerd");
  }
  
  // Simple instructie tekst
  g->setTextColor(COL_TX);
  g->setCursor(10, 70);
  if (selectedFile.length() > 0) {
    g->print("Kies ANALYSE voor data analyse");
    g->setCursor(10, 85);
    g->print("of TRAIN AI voor interactieve training");
  } else {
    g->setTextColor(0xF800);  // Rood
    g->print("Geen bestand geselecteerd.");
    g->setCursor(10, 85);
    g->setTextColor(COL_TX);
    g->print("Ga eerst naar Play menu.");
  }
  
  // Buttons - 3 knoppen horizontaal (adjusted for bigger buttons)
  int16_t btnY = SCR_H - BTN_H - 8;
  int16_t totalBtnWidth = 3 * BTN_W;
  int16_t availableSpace = SCR_W - totalBtnWidth;
  int16_t btnSpacing = availableSpace / 4;  // Space between and around buttons
  
  btnAnalyzeX = btnSpacing;
  btnAnalyzeY = btnY;
  
  // TRAIN button
  static int16_t btnTrainX = btnSpacing + BTN_W + btnSpacing;
  static int16_t btnTrainY = btnY;
  
  btnBackX = btnSpacing + (2 * BTN_W) + (2 * btnSpacing);
  btnBackY = btnY;
  
  uint16_t analyzeColor = selectedFile.length() > 0 ? COL_BTN_ANALYZE : 0x39E7;  // Grijs als disabled
  drawButton(btnAnalyzeX, btnAnalyzeY, BTN_W, BTN_H, "ANALYSE", analyzeColor);
  
  uint16_t trainColor = selectedFile.length() > 0 ? 0x07FF : 0x39E7;  // Helder cyaan als enabled, grijs als disabled
  drawButton(btnTrainX, btnTrainY, BTN_W, BTN_H, "TRAIN AI", trainColor);
  
  drawButton(btnBackX, btnBackY, BTN_W, BTN_H, "TERUG", COL_BTN_BACK);
  
  Serial.println("[AI] drawFileSelection() completed successfully");
}

static void analyzeFile(const String& filename) {
  Serial.printf("[AI] Analyzing file: %s\n", filename.c_str());
  
  // Reset result
  memset(&currentResult, 0, sizeof(currentResult));
  strncpy(currentResult.filename, filename.c_str(), sizeof(currentResult.filename) - 1);
  
  File file = SD.open("/" + filename);
  if (!file) {
    Serial.println("[AI] ERROR: Cannot open file!");
    strcpy(currentResult.analysis, "Error: Kan bestand niet openen");
    return;
  }
  
  Serial.printf("[AI] File opened successfully, size: %d bytes\n", file.size());
  
  // Skip header
  String header = file.readStringUntil('\n');
  Serial.printf("[AI] Header: %s\n", header.c_str());
  
  float totalHeartRate = 0;
  float totalTrust = 0;
  float totalSleeve = 0;
  float totalTemperature = 0;
  float totalGSR = 0;
  uint32_t validSamples = 0;
  int lineCount = 0;
  
  // Analyse CSV data: Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril
  while (file.available()) {
    String line = file.readStringUntil('\n');
    lineCount++;
    if (line.length() < 5) continue;
    
    // Debug first few lines
    if (lineCount <= 3) {
      Serial.printf("[AI] Line %d: %s\n", lineCount, line.c_str());
    }
    
    // Parse CSV line - support 12 fields: Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril
    int commaIndex[11];
    int commaCount = 0;
    for (int i = 0; i < line.length() && commaCount < 11; i++) {
      if (line.charAt(i) == ',') {
        commaIndex[commaCount++] = i;
      }
    }
    
    // Debug comma parsing for first few lines
    if (lineCount <= 3) {
      Serial.printf("[AI] Line %d has %d commas\n", lineCount, commaCount);
    }
    
    if (commaCount >= 7) {  // Need at least 8 fields (Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve) - need 7 commas for 8 fields
      // Extract values
      uint32_t timeMs = line.substring(0, commaIndex[0]).toInt();
      float heartRate = line.substring(commaIndex[0] + 1, commaIndex[1]).toFloat();
      float temperature = line.substring(commaIndex[1] + 1, commaIndex[2]).toFloat();
      float gsr = line.substring(commaIndex[2] + 1, commaIndex[3]).toFloat();
      float trust = line.substring(commaIndex[5] + 1, commaIndex[6]).toFloat();
      float sleeve = line.substring(commaIndex[6] + 1, commaIndex[7]).toFloat();
      
      // Debug first valid sample (after new validation)
      if (validSamples == 0) {
        Serial.printf("[AI] First valid sample - HR:%.1f T:%.1f GSR:%.1f Trust:%.1f Sleeve:%.1f\n", 
                     heartRate, temperature, gsr, trust, sleeve);
      }
      
      // More flexible validation - allow zero values and dummy data
      bool validData = (heartRate >= 0 && heartRate <= 200) && 
                       (temperature >= 0 && temperature <= 50) && 
                       (gsr >= 0 && gsr <= 2000);
      
      if (validData) {  // Valid sensor range
        totalHeartRate += heartRate;
        totalTemperature += temperature;
        totalGSR += gsr;
        totalTrust += trust;
        totalSleeve += sleeve;
        validSamples++;
        
        // Store total duration
        if (timeMs > currentResult.totalDurationMs) {
          currentResult.totalDurationMs = timeMs;
        }
        
        // Detect events and add to timeline
        if (heartRate > aiConfig.hrHighThreshold) {
          currentResult.highHREvents++;
          float severity = (heartRate - aiConfig.hrHighThreshold) / (200.0f - aiConfig.hrHighThreshold);
          addEvent(timeMs, 0, min(1.0f, severity));  // Type 0: Hoge hartslag
        }
        
        if (heartRate < aiConfig.hrLowThreshold) {
          currentResult.lowHREvents++;
          float severity = (aiConfig.hrLowThreshold - heartRate) / (aiConfig.hrLowThreshold - 30.0f);
          addEvent(timeMs, 3, min(1.0f, severity));  // Type 3: Lage hartslag
        }
        
        if (temperature > aiConfig.tempHighThreshold) {
          currentResult.highTempEvents++;
          float severity = (temperature - aiConfig.tempHighThreshold) / (40.0f - aiConfig.tempHighThreshold);
          addEvent(timeMs, 1, min(1.0f, severity));  // Type 1: Hoge temperatuur
        }
        
        if (gsr > aiConfig.gsrHighThreshold) {
          currentResult.highGSREvents++;
          float severity = (gsr - aiConfig.gsrHighThreshold) / (1000.0f - aiConfig.gsrHighThreshold);
          addEvent(timeMs, 2, min(1.0f, severity));  // Type 2: GSR stress
        }
        
        // Machine speed events will be detected in second pass after averages are calculated
        
        // Track max speeds
        if (trust > currentResult.maxTrustSpeed) currentResult.maxTrustSpeed = trust;
        if (sleeve > currentResult.maxSleeveSpeed) currentResult.maxSleeveSpeed = sleeve;
      }
    }
  }
  file.close();
  
  Serial.printf("[AI] Parsing complete: %d total lines, %d valid samples\n", lineCount, validSamples);
  
  if (validSamples > 0) {
    currentResult.totalSamples = validSamples;
    currentResult.avgHeartRate = totalHeartRate / validSamples;
    currentResult.avgTrustSpeed = totalTrust / validSamples;
    currentResult.avgSleeveSpeed = totalSleeve / validSamples;
    
    // Calculate recommendations based on events
    float eventRatio = float(currentResult.highHREvents + currentResult.highTempEvents + currentResult.highGSREvents) / validSamples;
    
    if (eventRatio > 0.1f) {  // More than 10% problematic samples
      currentResult.recommendedTrustReduction = 0.6f;  // Stronger reduction
      currentResult.recommendedSleeveReduction = 0.7f;
    } else if (eventRatio > 0.05f) {  // 5-10% problematic
      currentResult.recommendedTrustReduction = 0.75f;
      currentResult.recommendedSleeveReduction = 0.8f;
    } else {  // Less than 5% problematic
      currentResult.recommendedTrustReduction = 0.9f;
      currentResult.recommendedSleeveReduction = 0.9f;
    }
    
    // Second pass: detect machine speed events now that we have averages
    file.seek(0);
    file.readStringUntil('\n');  // Skip header again
    
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() < 5) continue;
      
      // Parse time and machine speeds only
      int commaIndex[11];
      int commaCount = 0;
      for (int i = 0; i < line.length() && commaCount < 11; i++) {
        if (line.charAt(i) == ',') {
          commaIndex[commaCount++] = i;
        }
      }
      
      if (commaCount >= 7) {  // Need 7 commas for 8 fields in second pass too
        uint32_t timeMs = line.substring(0, commaIndex[0]).toInt();
        float trust = line.substring(commaIndex[5] + 1, commaIndex[6]).toFloat();
        float sleeve = line.substring(commaIndex[6] + 1, commaIndex[7]).toFloat();
        
        // Detect machine speed spikes
        if (trust > currentResult.avgTrustSpeed * 1.8f || sleeve > currentResult.avgSleeveSpeed * 1.8f) {
          float severity = max(trust / currentResult.maxTrustSpeed, sleeve / currentResult.maxSleeveSpeed);
          addEvent(timeMs, 4, min(1.0f, severity));  // Type 4: Machine pieken
        }
        
        // Detect combined stress events (multiple factors)
        if (currentResult.eventCount > 0) {
          // Check if this time has multiple events within 5 seconds
          int nearbyEvents = 0;
          for (int i = 0; i < currentResult.eventCount; i++) {
            if (abs((int32_t)currentResult.events[i].timeMs - (int32_t)timeMs) < 5000) {
              nearbyEvents++;
            }
          }
          if (nearbyEvents >= 2) {
            addEvent(timeMs, 5, 0.8f);  // Type 5: Combined stress
          }
        }
      }
    }
    
    // Generate analysis text - improved for zero/dummy values
    if (currentResult.avgHeartRate < 10) {
      sprintf(currentResult.analysis, 
        "Analyse van %d samples:\n"
        "- Data type: Dummy/Test data\n"
        "- Hartslag: %.1f (zeer laag/dummy)\n"
        "- Temperatuur: %.1f C\n"
        "- GSR gemiddeld: %.1f\n"
        "- Machine snelheden: T=%.1f S=%.1f\n"
        "- Events gedetecteerd: %d\n"
        "\n"
        "STATUS: Test data geanalyseerd\n"
        "Echte sensor data aanbevolen\n"
        "voor accurate analyse.",
        validSamples,
        currentResult.avgHeartRate,
        totalTemperature / validSamples,  // Avg temperature
        totalGSR / validSamples,         // Avg GSR
        currentResult.maxTrustSpeed,
        currentResult.maxSleeveSpeed,
        currentResult.eventCount
      );
    } else {
      sprintf(currentResult.analysis, 
        "Analyse van %d samples:\n"
        "- Gem. hartslag: %.1f BPM\n"
        "- Risico events: %d (%.1f%%)\n"
        "- Max snelheden: T=%.1f S=%.1f\n"
        "- Timeline events: %d\n"
        "\n"
        "ADVIES:\n"
        "Trust reductie: %.1f%%\n"
        "Sleeve reductie: %.1f%%",
        validSamples,
        currentResult.avgHeartRate,
        currentResult.highHREvents + currentResult.highTempEvents + currentResult.highGSREvents,
        eventRatio * 100.0f,
        currentResult.maxTrustSpeed,
        currentResult.maxSleeveSpeed,
        currentResult.eventCount,
        (1.0f - currentResult.recommendedTrustReduction) * 100.0f,
        (1.0f - currentResult.recommendedSleeveReduction) * 100.0f
      );
    }
  } else {
    strcpy(currentResult.analysis, "Error: Geen geldige data gevonden");
  }
  
  analysisComplete = true;
}

static void drawResults() {
  g->fillScreen(COL_BG);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  g->setCursor(10, 12);
  g->printf("Analyse Resultaten: %s", currentResult.filename);
  
  // Results box
  g->fillRect(10, 30, SCR_W-20, SCR_H-80, COL_RESULT_BG);
  g->drawRect(10, 30, SCR_W-20, SCR_H-80, COL_FR);
  
  // Analysis text
  g->setTextColor(COL_TX);
  g->setCursor(15, 40);
  
  // Split analysis text into lines and display
  String analysisText = String(currentResult.analysis);
  int yPos = 40;
  int startPos = 0;
  
  while (startPos < analysisText.length() && yPos < SCR_H - 60) {
    int endPos = analysisText.indexOf('\n', startPos);
    if (endPos == -1) endPos = analysisText.length();
    
    String line = analysisText.substring(startPos, endPos);
    g->setCursor(15, yPos);
    g->print(line);
    
    yPos += 12;
    startPos = endPos + 1;
  }
  
  // Buttons - adjusted for bigger button sizes
  int16_t btnY = SCR_H - BTN_H - 8;
  int16_t totalWidth = 3 * BTN_W;
  int16_t spacing = (SCR_W - totalWidth) / 4;
  
  btnPrevX = spacing; btnPrevY = btnY;
  btnNextX = spacing + BTN_W + spacing; btnNextY = btnY;
  btnBackX = spacing + (2 * BTN_W) + (2 * spacing); btnBackY = btnY;
  
  drawButton(btnPrevX, btnPrevY, BTN_W, BTN_H, "TEKST", COL_BTN_PREV);
  drawButton(btnNextX, btnNextY, BTN_W, BTN_H, "TIMELINE", COL_BTN_NEXT);
  drawButton(btnBackX, btnBackY, BTN_W, BTN_H, "TERUG", COL_BTN_BACK);
}

static void drawTimeline() {
  g->fillScreen(COL_BG);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  g->setCursor(10, 12);
  g->printf("AI Timeline: %s", currentResult.filename);
  
  // Timeline info
  g->setCursor(10, 30);
  g->printf("Duur: %.1f min | Events: %d | Focus: laatste 33%%", 
            currentResult.totalDurationMs / 60000.0f, currentResult.eventCount);
  
  // Instructions for interactive timeline
  g->setTextColor(0xFFE0);  // Geel
  g->setTextSize(1);
  g->setCursor(10, 42);
  g->print("Tip: Tik op event → TRAIN AI om de AI te leren wat dit moment betekent");
  
  // Timeline area
  int16_t timelineX = 10;
  int16_t timelineY = 50;
  int16_t timelineW = SCR_W - 20;
  int16_t timelineH = 80;
  
  // Timeline background
  g->fillRect(timelineX, timelineY, timelineW, timelineH, 0x2104);
  g->drawRect(timelineX, timelineY, timelineW, timelineH, COL_FR);
  
  // Mark the last third (most important)
  int16_t lastThirdX = timelineX + (timelineW * 2 / 3);
  g->drawFastVLine(lastThirdX, timelineY, timelineH, 0xFFE0);  // Gele lijn
  g->setTextColor(0xFFE0);
  g->setTextSize(1);
  g->setCursor(lastThirdX + 2, timelineY + 2);
  g->print("Kritiek");
  
  // Calculate visible time range based on scroll
  uint32_t totalTime = currentResult.totalDurationMs;
  uint32_t viewStart = (uint32_t)(timelineScroll * totalTime * 0.8f);  // Can scroll 80% of timeline
  uint32_t viewEnd = viewStart + (totalTime / 3);  // Show 1/3 of timeline
  
  // Draw time markers
  g->setTextColor(COL_TX);
  for (int i = 0; i <= 4; i++) {
    int16_t x = timelineX + (i * timelineW / 4);
    uint32_t time = viewStart + (i * (viewEnd - viewStart) / 4);
    g->drawFastVLine(x, timelineY + timelineH - 5, 5, COL_TX);
    g->setCursor(x - 10, timelineY + timelineH + 8);
    g->printf("%.1f", time / 60000.0f);  // Minutes
  }
  
  // Draw events - now clickable for training
  for (int i = 0; i < currentResult.eventCount; i++) {
    const EventData& event = currentResult.events[i];
    
    // Check if event is in visible range
    if (event.timeMs >= viewStart && event.timeMs <= viewEnd) {
      // Calculate X position
      float normalizedTime = float(event.timeMs - viewStart) / float(viewEnd - viewStart);
      int16_t eventX = timelineX + (int16_t)(normalizedTime * timelineW);
      
      // Event height based on severity
      int16_t eventHeight = (int16_t)(event.severity * (timelineH - 10)) + 5;
      int16_t eventY = timelineY + timelineH - eventHeight;
      
      // Draw event bar (wider for touch)
      uint16_t color = eventColors[event.eventType % MAX_EVENT_TYPES];
      g->fillRect(eventX - 2, eventY, 5, eventHeight, color);
      
      // Highlight events in last third
      if (event.timeMs > totalTime * 0.67f) {
        g->drawRect(eventX - 3, eventY - 1, 7, eventHeight + 2, 0xFFFF);  // White border
      }
      
      // Add touch indicator (small circle on top)
      g->fillCircle(eventX, eventY - 3, 2, color);
    }
  }
  
  // Color legend
  int16_t legendY = timelineY + timelineH + 25;
  g->setTextColor(COL_TX);
  g->setCursor(10, legendY);
  g->print("Legenda:");
  
  for (int i = 0; i < min(4, MAX_EVENT_TYPES); i++) {
    int16_t legendX = 10 + (i % 2) * 150;
    int16_t legendYPos = legendY + 15 + (i / 2) * 15;
    
    // Color box
    g->fillRect(legendX, legendYPos, 12, 8, eventColors[i]);
    g->drawRect(legendX, legendYPos, 12, 8, COL_FR);
    
    // Description - gebruik configureerbare namen
    g->setTextColor(COL_TX);
    g->setCursor(legendX + 15, legendYPos);
    String eventName = String(aiEventConfig_getName(i));
    if (eventName.length() > 20) {
      eventName = eventName.substring(0, 17) + "...";
    }
    g->print(eventName);
  }
  
  // Scroll indicator
  if (currentResult.eventCount > 0) {
    int16_t scrollBarY = SCR_H - 50;
    int16_t scrollBarW = SCR_W - 40;
    int16_t scrollBarH = 4;
    int16_t scrollBarX = 20;
    
    // Scroll bar background
    g->drawRect(scrollBarX, scrollBarY, scrollBarW, scrollBarH, COL_FR);
    
    // Scroll thumb
    int16_t thumbW = scrollBarW / 3;  // Showing 1/3 of timeline
    int16_t thumbX = scrollBarX + (int16_t)(timelineScroll * (scrollBarW - thumbW));
    g->fillRect(thumbX, scrollBarY, thumbW, scrollBarH, 0x07E0);
  }
  
  // Buttons - adjusted for bigger button sizes
  int16_t btnY = SCR_H - BTN_H - 8;
  int16_t totalWidth = 3 * BTN_W;
  int16_t spacing = (SCR_W - totalWidth) / 4;
  
  btnPrevX = spacing; btnPrevY = btnY;
  btnNextX = spacing + BTN_W + spacing; btnNextY = btnY;
  btnBackX = spacing + (2 * BTN_W) + (2 * spacing); btnBackY = btnY;
  
  drawButton(btnPrevX, btnPrevY, BTN_W, BTN_H, "VORIGE", COL_BTN_PREV);
  drawButton(btnNextX, btnNextY, BTN_W, BTN_H, "VOLGENDE", COL_BTN_NEXT);
  drawButton(btnBackX, btnBackY, BTN_W, BTN_H, "TERUG", COL_BTN_BACK);
}

void aiAnalyze_begin(Arduino_GFX* gfx) {
  Serial.println("[AI] aiAnalyze_begin() starting...");
  
  Serial.println("[AI] Setting up graphics...");
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  Serial.println("[AI] Initializing variables...");
  currentPage = 0;
  analysisComplete = false;
  // DON'T reset selectedFile here - it may have been set by playlist
  
  Serial.printf("[AI] Current selectedFile: '%s'\n", selectedFile.c_str());
  
  Serial.println("[AI] Drawing file selection screen...");
  drawFileSelection();
  
  Serial.println("[AI] aiAnalyze_begin() completed successfully");
}

AnalyzeEvent aiAnalyze_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return AE_NONE;
  
  uint32_t now = millis();
  if (now - lastTouchMs < ANALYZE_COOLDOWN_MS) return AE_NONE;
  
  if (currentPage == 0) {  // File selection page
    
    if (inRect(x, y, btnAnalyzeX, btnAnalyzeY, BTN_W, BTN_H) && selectedFile.length() > 0) {
      lastTouchMs = now;
      analyzeFile(selectedFile);
      currentPage = 1;
      drawResults();
      return AE_NONE;
    }
    
    // TRAIN AI button - gebruik dezelfde coordinates als in drawFileSelection
    int16_t totalBtnWidth = 3 * BTN_W;
    int16_t availableSpace = SCR_W - totalBtnWidth;
    int16_t btnSpacing = availableSpace / 4;
    int16_t btnTrainX = btnSpacing + BTN_W + btnSpacing;
    int16_t btnTrainY = SCR_H - BTN_H - 8;
    if (inRect(x, y, btnTrainX, btnTrainY, BTN_W, BTN_H) && selectedFile.length() > 0) {
      lastTouchMs = now;
      return AE_TRAIN;
    }
    
    if (inRect(x, y, btnBackX, btnBackY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      return AE_BACK;
    }
  } else if (currentPage == 1) {  // Results text page
    if (inRect(x, y, btnPrevX, btnPrevY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      // Go back to text view (already here)
      return AE_NONE;
    }
    
    if (inRect(x, y, btnNextX, btnNextY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      currentPage = 2;  // Go to timeline
      timelineScroll = 0.7f;  // Start at 70% (focus on last third)
      drawTimeline();
      return AE_NONE;
    }
    
    if (inRect(x, y, btnBackX, btnBackY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      currentPage = 0;
      drawFileSelection();
      return AE_NONE;
    }
  } else if (currentPage == 2) {  // Timeline page
    if (inRect(x, y, btnPrevX, btnPrevY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      float newScroll = max(0.0f, timelineScroll - 0.2f);
      if (newScroll != timelineScroll && now - lastTimelineUpdate > 200) {  // Anti-flicker: max 5Hz
        timelineScroll = newScroll;
        drawTimeline();
        lastTimelineUpdate = now;
      }
      return AE_NONE;
    }
    
    if (inRect(x, y, btnNextX, btnNextY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      float newScroll = min(1.0f, timelineScroll + 0.2f);
      if (newScroll != timelineScroll && now - lastTimelineUpdate > 200) {  // Anti-flicker: max 5Hz
        timelineScroll = newScroll;
        drawTimeline();
        lastTimelineUpdate = now;
      }
      return AE_NONE;
    }
    
    if (inRect(x, y, btnBackX, btnBackY, BTN_W, BTN_H)) {
      lastTouchMs = now;
      currentPage = 1;  // Back to results text
      drawResults();
      return AE_NONE;
    }
    
    // Check for event clicks in timeline area
    int16_t timelineX = 10;
    int16_t timelineY = 50;
    int16_t timelineW = SCR_W - 20;
    int16_t timelineH = 80;
    
    if (inRect(x, y, timelineX, timelineY, timelineW, timelineH)) {
      // Check if click is on an event for training
      uint32_t totalTime = currentResult.totalDurationMs;
      uint32_t viewStart = (uint32_t)(timelineScroll * totalTime * 0.8f);
      uint32_t viewEnd = viewStart + (totalTime / 3);
      
      for (int i = 0; i < currentResult.eventCount; i++) {
        const EventData& event = currentResult.events[i];
        
        if (event.timeMs >= viewStart && event.timeMs <= viewEnd) {
          // Calculate event position
          float normalizedTime = float(event.timeMs - viewStart) / float(viewEnd - viewStart);
          int16_t eventX = timelineX + (int16_t)(normalizedTime * timelineW);
          int16_t eventHeight = (int16_t)(event.severity * (timelineH - 10)) + 5;
          int16_t eventY = timelineY + timelineH - eventHeight;
          
          // Check if touch is near this event (touch area: ±8 pixels)
          if (abs(x - eventX) <= 8 && y >= eventY - 8 && y <= timelineY + timelineH + 5) {
            lastTouchMs = now;
            Serial.printf("[AI] Event clicked at time %.1f min, type %d, severity %.2f\n", 
                         event.timeMs / 60000.0f, event.eventType, event.severity);
            return AE_TRAIN;  // Launch training for this event
          }
        }
      }
    }
  }
  
  return AE_NONE;
}

// Functions for external file management
String aiAnalyze_getSelectedFile() {
  return selectedFile;
}

void aiAnalyze_setSelectedFile(const String& filename) {
  selectedFile = filename;
  Serial.printf("[AI] Selected file set to: %s\n", filename.c_str());
  // DON'T draw here - graphics may not be initialized yet!
  // The screen will be drawn when aiAnalyze_begin() is called
}
