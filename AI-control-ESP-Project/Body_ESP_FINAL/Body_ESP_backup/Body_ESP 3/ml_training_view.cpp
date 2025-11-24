/*
  ML Training View Implementation
  
  Complete graphical interface for ML model training workflow
*/

#include "ml_training_view.h"
#include "ml_stress_analyzer.h"
#include "ml_trainer.h"
#include "ml_data_parser.h"
#include "input_touch.h"
#include <SD.h>
#include "body_config.h"
#include "body_display.h"
#include "body_gfx4.h"
#include "body_fonts.h"

// External function declarations (defined in Body_ESP.ino)
extern void startRecording();
extern void stopRecording();
extern bool isRecording;

// ===== Global Variables =====
static Arduino_GFX *g = nullptr;
MLTrainingState currentMLState = ML_STATE_MAIN;
int selectedFileIndex = -1;
int selectedModelIndex = -1;
MLTrainingProgress trainingProgress;

// File and model lists
static MLFileInfo fileList[10];  // Max 10 .aly files
static MLModelInfo modelList[5]; // Max 5 .mlm files  
static int fileCount = 0;
static int modelCount = 0;
static int scrollOffset = 0;

// Touch state
static bool touchPressed = false;
static uint32_t lastTouchTime = 0;
static const uint32_t TOUCH_DEBOUNCE = 200; // 200ms debounce

// ===== Initialization =====

bool mlTraining_begin(Arduino_GFX *gfx) {
  g = gfx;
  currentMLState = ML_STATE_MAIN;
  selectedFileIndex = -1;
  selectedModelIndex = -1;
  scrollOffset = 0;
  
  Serial.println("[ML TRAINING] Initializing ML Training view...");
  
  // AUTOMATIC CSV RECORDING: Start recording when entering ML Training mode
  if (BODY_CFG.autoRecordSessions && !isRecording) {
    startRecording();
    Serial.println("[ML TRAINING] Auto-recording started for ML Training session");
  }
  
  // Scan for .aly files and models
  mlTraining_scanAlyFiles();
  mlTraining_scanModels();
  
  mlTraining_drawMainMenu();
  
  Serial.printf("[ML TRAINING] Found %d .aly files, %d models\\n", fileCount, modelCount);
  return true;
}

// ===== Main Event Polling =====

MLTrainingEvent mlTraining_poll() {
  int16_t tx, ty;
  if (!inputTouchRead(tx, ty)) {
    return MTE_NONE;
  }
  
  // Debounce check
  uint32_t now = millis();
  if (now - lastTouchTime < TOUCH_DEBOUNCE) {
    return MTE_NONE;
  }
  lastTouchTime = now;
  
  // Handle touch based on current state
  switch (currentMLState) {
    case ML_STATE_MAIN:
      return handleMainMenuTouch(tx, ty);
    case ML_STATE_IMPORT:
      return handleImportTouch(tx, ty);
    case ML_STATE_TRAINING:
      return handleTrainingTouch(tx, ty);
    case ML_STATE_MODEL_MANAGER:
      return handleModelManagerTouch(tx, ty);
    case ML_STATE_AI_ANNOTATE:
      return handleAIAnnotateTouch(tx, ty);
    case ML_STATE_ANNOTATE_REVIEW:
      return handleAnnotateReviewTouch(tx, ty);
    default:
      // For other states, just handle back button
      if (ty > 190 && tx > 230 && tx < 310) { // Back button area
        return MTE_BACK;
      }
      break;
  }
  
  return MTE_NONE;
}

// ===== State Management =====

void mlTraining_setState(MLTrainingState newState) {
  if (currentMLState != newState) {
    Serial.printf("[ML TRAINING] State change: %d -> %d\\n", currentMLState, newState);
    currentMLState = newState;
    
    // Draw appropriate screen for new state
    switch (newState) {
      case ML_STATE_MAIN:
        mlTraining_drawMainMenu();
        break;
      case ML_STATE_IMPORT:
        mlTraining_drawImportScreen();
        break;
      case ML_STATE_PREVIEW:
        mlTraining_drawPreviewScreen();
        break;
      case ML_STATE_TRAINING:
        mlTraining_drawTrainingScreen();
        break;
      case ML_STATE_MODEL_MANAGER:
        mlTraining_drawModelManager();
        break;
      case ML_STATE_MODEL_INFO:
        mlTraining_drawModelInfo();
        break;
      case ML_STATE_AI_ANNOTATE:
        mlTraining_drawAIAnnotateScreen();
        break;
      case ML_STATE_ANNOTATE_REVIEW:
        mlTraining_drawAnnotateReviewScreen();
        break;
    }
  }
}

MLTrainingState mlTraining_getState() {
  return currentMLState;
}

// ===== File Operations =====

// Forward declaration
bool mlTraining_parseAlyFile(MLFileInfo &info);

bool mlTraining_scanAlyFiles() {
  fileCount = 0;
  
  if (!SD.begin()) {
    Serial.println("[ML TRAINING] SD card not available");
    return false;
  }
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("[ML TRAINING] Cannot open root directory");
    return false;
  }
  
  File file = root.openNextFile();
  while (file && fileCount < 10) {
    String filename = file.name();
    
    if (filename.endsWith(".aly")) {
      MLFileInfo &info = fileList[fileCount];
      info.filename = filename;
      info.fullPath = "/" + filename;
      info.fileSize = file.size();
      info.isValid = true; // TODO: Validate file format
      
      // Parse file to get sample count and stats
      mlTraining_parseAlyFile(info);
      
      fileCount++;
      Serial.printf("[ML TRAINING] Found .aly file: %s (%d bytes)\\n", 
                    filename.c_str(), info.fileSize);
    }
    
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return true;
}

bool mlTraining_parseAlyFile(MLFileInfo &info) {
  File file = SD.open(info.fullPath);
  if (!file) {
    info.isValid = false;
    return false;
  }
  
  String line;
  int samples = 0;
  float confidenceSum = 0.0f;
  
  // Skip header line
  if (file.available()) {
    line = file.readStringUntil('\\n');
  }
  
  // Parse data lines
  while (file.available() && samples < 1000) { // Limit parsing for performance
    line = file.readStringUntil('\\n');
    line.trim();
    
    if (line.length() > 0) {
      // Parse CSV: Timestamp,HeartRate,Temperature,GSR,StressLevel,Confidence,Reasoning
      int commaIndex[6];
      int commaCount = 0;
      
      for (int i = 0; i < line.length() && commaCount < 6; i++) {
        if (line[i] == ',') {
          commaIndex[commaCount++] = i;
        }
      }
      
      if (commaCount >= 5) {
        // Extract stress level (index 4)
        String stressStr = line.substring(commaIndex[3] + 1, commaIndex[4]);
        int stressLevel = stressStr.toInt();
        
        if (stressLevel >= 1 && stressLevel <= 7) {
          info.stressLevels[stressLevel - 1]++;
        }
        
        // Extract confidence (index 5)
        String confStr = line.substring(commaIndex[4] + 1, commaIndex[5]);
        float confidence = confStr.toFloat();
        confidenceSum += confidence;
        
        samples++;
      }
    }
  }
  
  file.close();
  
  info.sampleCount = samples;
  info.avgConfidence = samples > 0 ? confidenceSum / samples : 0.0f;
  info.isValid = samples > 0;
  
  return info.isValid;
}

int mlTraining_getFileCount() {
  return fileCount;
}

MLFileInfo mlTraining_getFileInfo(int index) {
  if (index >= 0 && index < fileCount) {
    return fileList[index];
  }
  return MLFileInfo(); // Empty info
}

// ===== Model Management =====

bool mlTraining_scanModels() {
  modelCount = 0;
  
  // Add built-in rule-based model
  MLModelInfo &builtIn = modelList[modelCount++];
  builtIn.filename = "rule_based";
  builtIn.name = "Rule-based Classifier";
  builtIn.accuracy = 0.80f; // Estimated
  builtIn.trainingSize = 0;
  builtIn.trainedDate = "Built-in";
  builtIn.isBuiltIn = true;
  builtIn.isActive = !mlAnalyzer.hasModel(); // Active if no custom model loaded
  
  // Scan for .mlm files
  if (!SD.begin()) return false;
  
  File root = SD.open("/");
  if (!root) return false;
  
  File file = root.openNextFile();
  while (file && modelCount < 5) {
    String filename = file.name();
    
    if (filename.endsWith(".mlm")) {
      MLModelInfo &info = modelList[modelCount];
      info.filename = filename;
      info.name = filename.substring(0, filename.length() - 4); // Remove .mlm
      info.modelSize = file.size();
      info.isBuiltIn = false;
      info.isActive = false; // TODO: Check if currently loaded
      
      // TODO: Parse model file for accuracy and training info
      info.accuracy = 0.85f; // Placeholder
      info.trainingSize = 500; // Placeholder
      info.trainedDate = "Unknown"; // Placeholder
      
      modelCount++;
      Serial.printf("[ML TRAINING] Found model: %s (%d bytes)\\n", 
                    filename.c_str(), info.modelSize);
    }
    
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return true;
}

int mlTraining_getModelCount() {
  return modelCount;
}

MLModelInfo mlTraining_getModelInfo(int index) {
  if (index >= 0 && index < modelCount) {
    return modelList[index];
  }
  return MLModelInfo(); // Empty info
}

// ===== Drawing Functions =====

// Eenvoudige button functie in zelfde stijl als menu_view.cpp
static void drawMLButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* txt, uint16_t color) {
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_FR = 0xC618;
  
  // Eenvoudige button met afgeronde hoeken - zelfde stijl als andere menu's
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

void mlTraining_drawMainMenu() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  
  // Zelfde layout als andere menu's
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - zelfde stijl als menu_view.cpp  
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("ML Training", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 25);
  g->print("ML Training");
  
  // Button layout - exact zelfde als menu_view.cpp
  int16_t btnW = 200;  // Zelfde breedte
  int16_t btnH = 30;   // Iets hoger voor 4 knoppen
  int16_t btnX = (SCR_W - btnW) / 2;  // Gecentreerd
  int16_t startY = 55;  // Begin onder title
  int16_t gap = 8;     // Iets meer ruimte tussen knoppen
  
  // Button colors - verschillende kleuren voor elke functie
  const uint16_t COL_IMPORT = 0x07E0;   // Groen
  const uint16_t COL_TRAIN = 0xFFE0;    // Geel  
  const uint16_t COL_MODELS = 0xF81F;   // Magenta
  const uint16_t COL_INFO = 0x07FF;     // Cyan
  const uint16_t COL_BACK = 0x001F;     // Blauw
  
// Menu teksten - aangepast voor nieuwe workflow
  const char* menuItems[] = {
    "Train Model (.aly)",
    "AI Annotatie (.csv)", 
    "Model Manager",
    "Training Info"
  };
  
  // Teken knoppen in zelfde stijl
  drawMLButton(btnX, startY, btnW, btnH, menuItems[0], COL_IMPORT);
  drawMLButton(btnX, startY + (btnH + gap), btnW, btnH, menuItems[1], COL_TRAIN);
  drawMLButton(btnX, startY + (btnH + gap) * 2, btnW, btnH, menuItems[2], COL_MODELS);
  drawMLButton(btnX, startY + (btnH + gap) * 3, btnW, btnH, menuItems[3], COL_INFO);
  drawMLButton(btnX, startY + (btnH + gap) * 4, btnW, btnH, "TERUG", COL_BACK);
  
  // Status info onder knoppen - compacter
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  
  // Files en models count onderaan - goed binnen scherm
  g->setCursor(20, SCR_H - 30);
  g->printf("Files: %d  Models: %d", fileCount, modelCount);
  
  // Model status onderaan links
  g->setCursor(20, SCR_H - 15);
  if (mlAnalyzer.hasModel()) {
    g->print("Custom ML model");
  } else {
    g->print("Rule-based");
  }
}

void mlTraining_drawImportScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  const uint16_t COL_SELECT = 0x07E0; // Groen voor selectie
  
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - zelfde stijl
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("Import Data", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("Import Data");
  
  // Bestandslijst
  int y = 50;
  
  if (fileCount == 0) {
    g->setTextSize(1);
    g->setTextColor(COL_TX);
    g->setCursor((SCR_W - 120) / 2, y + 30);
    g->print("Geen .aly bestanden");
    g->setCursor((SCR_W - 180) / 2, y + 50);
    g->print("Gebruik AI Analyze eerst");
  } else {
    // Toon bestanden in lijst - compacter
    g->setTextSize(1);
    for (int i = 0; i < fileCount && y < (SCR_H - 80); i++) {
      MLFileInfo info = fileList[i];
      
      // Selectie achtergrond
      if (i == selectedFileIndex) {
        g->fillRoundRect(10, y - 2, SCR_W - 20, 28, 4, COL_SELECT);
        g->setTextColor(BLACK);
      } else {
        g->setTextColor(COL_TX);
      }
      
      // Bestandsnaam
      g->setCursor(15, y + 2);
      char shortName[25];
      strncpy(shortName, info.filename.c_str(), 24);
      shortName[24] = '\0';
      g->print(shortName);
      
      // Samples rechts
      char sampleText[15];
      snprintf(sampleText, sizeof(sampleText), "%d smp", info.sampleCount);
      g->getTextBounds(sampleText, 0, 0, &x1, &y1, &tw, &th);
      g->setCursor(SCR_W - tw - 15, y + 2);
      g->print(sampleText);
      
      // Bestandsgrootte en confidence onder elkaar
      g->setCursor(15, y + 14);
      g->printf("%.1fkB", info.fileSize / 1024.0f);
      
      char confText[12];
      snprintf(confText, sizeof(confText), "%.0f%%", info.avgConfidence * 100);
      g->getTextBounds(confText, 0, 0, &x1, &y1, &tw, &th);
      g->setCursor(SCR_W - tw - 15, y + 14);
      g->print(confText);
      
      y += 32;
    }
  }
  
  // Knoppen onderaan - zelfde stijl als andere schermen
  int btnY = SCR_H - 50;
  int btnW = 75;
  int btnH = 30;
  int spacing = 10;
  
  // Import knop - alleen actief als bestand geselecteerd
  uint16_t importColor = (selectedFileIndex >= 0) ? COL_SELECT : 0x4A49; // Grijs als inactief
  drawMLButton(10, btnY, btnW, btnH, "Import", importColor);
  
  // Preview knop
  drawMLButton(10 + btnW + spacing, btnY, btnW, btnH, "Preview", 0xFFE0); // Geel
  
  // Back knop
  drawMLButton(SCR_W - btnW - 10, btnY, btnW, btnH, "Terug", 0x001F); // Blauw
  
  // Status info
  if (fileCount > 0) {
    g->setTextColor(COL_TX);
    g->setTextSize(1);
    g->setCursor(10, 10);
    g->printf("%d bestanden", fileCount);
  }
}

void mlTraining_drawTrainingScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  const uint16_t COL_PROGRESS = 0xFFE0; // Geel voor progress
  const uint16_t COL_SUCCESS = 0x07E0;  // Groen voor succes
  const uint16_t COL_ERROR = 0xF800;    // Rood voor stop/error
  
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - gecentreerd
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("ML Training", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("ML Training");
  
  int y = 55;
  g->setTextSize(1);
  
  if (trainingProgress.isTraining) {
    // Progress bar - zelfde stijl als knoppen
    int barW = SCR_W - 40;
    int barH = 25;
    int barX = 20;
    
    // Progress percentage
    float progress = (trainingProgress.processedSamples > 0) ?
      (float)trainingProgress.processedSamples / max(1, trainingProgress.totalSamples) : 0;
    
    g->drawRoundRect(barX, y, barW, barH, 8, COL_TX);
    int fillW = (int)((barW - 4) * progress);
    if (fillW > 0) {
      g->fillRoundRect(barX+2, y+2, fillW, barH-4, 6, COL_PROGRESS);
    }
    
    // Progress tekst in bar
    g->setTextColor(BLACK);
    char progressText[20];
    snprintf(progressText, sizeof(progressText), "%.1f%%", progress * 100);
    g->getTextBounds(progressText, 0, 0, &x1, &y1, &tw, &th);
    g->setCursor(barX + (barW - tw)/2, y + (barH + th)/2 - 2);
    g->print(progressText);
    
    y += 35;
    
    // Training details - compact
    g->setTextColor(COL_TX);
    g->setCursor(20, y);
    g->printf("Samples: %d / %d", trainingProgress.processedSamples, trainingProgress.totalSamples);
    
    y += 15;
    g->setCursor(20, y);
    g->printf("Phase: %s", trainingProgress.currentPhase.c_str());
    
    y += 15;
    g->setCursor(20, y);
    g->printf("Accuracy: %.1f%%", trainingProgress.currentAccuracy * 100);
    
    y += 15;
    uint32_t elapsed = (millis() - trainingProgress.startTime) / 1000;
    g->setCursor(20, y);
    g->printf("Time: %dm %ds", elapsed/60, elapsed%60);
    
  } else if (trainingProgress.isComplete) {
    // Training voltooid - gecentreerd
    g->setTextColor(COL_SUCCESS);
    g->setTextSize(1);
    g->getTextBounds("Training Voltooid!", 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, y);
    g->print("Training Voltooid!");
    
    y += 25;
    
    // Resultaten
    g->setTextColor(COL_TX);
    g->setCursor(20, y);
    g->printf("Eindaccuracy: %.1f%%", trainingProgress.currentAccuracy * 100);
    
    y += 15;
    g->setCursor(20, y);
    g->printf("Training samples: %d", trainingProgress.totalSamples);
    
    y += 15;
    uint32_t totalTime = (millis() - trainingProgress.startTime) / 1000;
    g->setCursor(20, y);
    g->printf("Totale tijd: %dm %ds", totalTime/60, totalTime%60);
    
  } else {
    // Klaar om te trainen
    g->setTextColor(COL_TX);
    g->setCursor(20, y);
    g->print("Klaar om ML model te trainen");
    
    y += 20;
    g->setCursor(20, y);
    g->print("Zorg dat je .aly data hebt geimporteerd");
  }
  
  // Knoppen onderaan
  int btnY = SCR_H - 45;
  int btnW = 85;
  int btnH = 35;
  int spacing = 15;
  
  if (trainingProgress.isTraining) {
    // Stop knop
    drawMLButton((SCR_W - btnW) / 2, btnY, btnW, btnH, "STOP", COL_ERROR);
    
  } else if (trainingProgress.isComplete) {
    // Save en Discard knoppen
    int btn1X = (SCR_W - (btnW * 2 + spacing)) / 2;
    drawMLButton(btn1X, btnY, btnW, btnH, "OPSLAAN", COL_SUCCESS);
    drawMLButton(btn1X + btnW + spacing, btnY, btnW, btnH, "WEGGOOIEN", COL_ERROR);
    
  } else {
    // Start knop
    drawMLButton((SCR_W - btnW) / 2, btnY, btnW, btnH, "START", COL_SUCCESS);
  }
  
  // Back knop rechtsboven
  drawMLButton(SCR_W - 75, 10, 65, 25, "Terug", 0x001F);
}

void mlTraining_drawModelManager() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  const uint16_t COL_SELECT = 0x07E0;  // Groen voor selectie
  const uint16_t COL_ACTIVE = 0x07E0;  // Groen voor actief model
  
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - gecentreerd
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("Model Manager", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("Model Manager");
  
  // Model lijst
  int y = 55;
  g->setTextSize(1);
  
  if (modelCount == 0) {
    g->setTextColor(COL_TX);
    g->setCursor((SCR_W - 120) / 2, y + 30);
    g->print("Geen modellen gevonden");
  } else {
    for (int i = 0; i < modelCount && y < (SCR_H - 80); i++) {
      MLModelInfo info = modelList[i];
      
      // Selectie achtergrond
      if (i == selectedModelIndex) {
        g->fillRoundRect(10, y - 2, SCR_W - 20, 30, 4, COL_SELECT);
        g->setTextColor(BLACK);
      } else {
        g->setTextColor(COL_TX);
      }
      
      // Active indicator - groene stip
      if (info.isActive) {
        g->fillCircle(18, y + 8, 3, COL_ACTIVE);
      } else {
        g->drawCircle(18, y + 8, 3, (i == selectedModelIndex) ? BLACK : COL_TX);
      }
      
      // Model naam
      g->setCursor(30, y + 2);
      char shortName[20];
      strncpy(shortName, info.name.c_str(), 19);
      shortName[19] = '\0';
      g->print(shortName);
      
      // Accuracy rechts
      char accText[10];
      snprintf(accText, sizeof(accText), "%.1f%%", info.accuracy * 100);
      g->getTextBounds(accText, 0, 0, &x1, &y1, &tw, &th);
      g->setCursor(SCR_W - tw - 15, y + 2);
      g->print(accText);
      
      // Details onder
      g->setCursor(30, y + 14);
      if (info.isBuiltIn) {
        g->print("Ingebouwd");
      } else {
        g->printf("%d smp, %.0fKB", info.trainingSize, info.modelSize/1024.0f);
      }
      
      y += 34;
    }
  }
  
  // Knoppen onderaan
  int btnY = SCR_H - 45;
  int btnW = 65;
  int btnH = 30;
  int spacing = 8;
  
  // Bereken totale breedte van alle knoppen
  int totalWidth = (btnW * 4) + (spacing * 3);
  int startX = (SCR_W - totalWidth) / 2;
  
  // Load knop - alleen actief als model geselecteerd
  uint16_t loadColor = (selectedModelIndex >= 0) ? COL_SELECT : 0x4A49;
  drawMLButton(startX, btnY, btnW, btnH, "Load", loadColor);
  
  // Delete knop - alleen voor custom models
  bool canDelete = (selectedModelIndex >= 0 && !modelList[selectedModelIndex].isBuiltIn);
  uint16_t deleteColor = canDelete ? 0xF800 : 0x4A49; // Rood of grijs
  drawMLButton(startX + (btnW + spacing), btnY, btnW, btnH, "Delete", deleteColor);
  
  // Info knop
  drawMLButton(startX + (btnW + spacing) * 2, btnY, btnW, btnH, "Info", 0xFFE0); // Geel
  
  // Back knop
  drawMLButton(startX + (btnW + spacing) * 3, btnY, btnW, btnH, "Terug", 0x001F); // Blauw
  
  // Status info
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  g->setCursor(10, 10);
  g->printf("%d modellen", modelCount);
}

void mlTraining_drawPreviewScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - gecentreerd
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("File Preview", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("File Preview");
  
  // Placeholder content
  g->setTextSize(1);
  g->setTextColor(COL_TX);
  g->setCursor((SCR_W - 180) / 2, 80);
  g->print("Preview functionaliteit");
  g->setCursor((SCR_W - 120) / 2, 100);
  g->print("komt binnenkort...");
  
  // Back knop
  drawMLButton(SCR_W - 75, 10, 65, 25, "Terug", 0x001F);
}

void mlTraining_drawModelInfo() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  
  //int16_t SCR_W = g->width();
  //int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  // Title - gecentreerd
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("Model Info", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("Model Info");
  
  // Placeholder content
  g->setTextSize(1);
  g->setTextColor(COL_TX);
  g->setCursor((SCR_W - 140) / 2, 80);
  g->print("Model details");
  g->setCursor((SCR_W - 120) / 2, 100);
  g->print("komt binnenkort...");
  
  // Back knop
  drawMLButton(SCR_W - 75, 10, 65, 25, "Terug", 0x001F);
}

// ===== Utility Functions =====

String mlTraining_formatFileSize(uint32_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024) + "KB";
  } else {
    return String(bytes / (1024 * 1024)) + "MB";
  }
}

String mlTraining_formatDuration(uint32_t seconds) {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  
  if (hours > 0) {
    return String(hours) + ":" + String(minutes) + ":" + String(secs);
  } else {
    return String(minutes) + ":" + String(secs);
  }
}

String mlTraining_formatAccuracy(float accuracy) {
  return String(accuracy * 100, 1) + "%";
}

// ===== Training Operations (Placeholder implementations) =====

bool mlTraining_startTraining() {
  Serial.println("[ML TRAINING] Starting training with MLTrainer...");
  
  // Check of we .aly bestanden hebben
  if (fileCount == 0 || selectedFileIndex < 0) {
    Serial.println("[ML TRAINING] Fout: geen .aly bestand geselecteerd");
    trainingProgress.hasError = true;
    trainingProgress.errorMessage = "Geen data geselecteerd";
    return false;
  }
  
  // Clear oude training data
  mlTrainer.clearData();
  
  // Load geselecteerd .aly bestand
  MLFileInfo info = fileList[selectedFileIndex];
  if (!mlTrainer.loadAlyFile(info.fullPath.c_str())) {
    Serial.println("[ML TRAINING] Fout: kan .aly niet laden");
    trainingProgress.hasError = true;
    trainingProgress.errorMessage = "Kan bestand niet laden";
    return false;
  }
  
  // Start training
  if (mlTrainer.startTraining()) {
    // Kopieer status van MLTrainer
    TrainingStatus status = mlTrainer.getStatus();
    trainingProgress.isTraining = status.isTraining;
    trainingProgress.isComplete = status.isComplete;
    trainingProgress.hasError = status.hasError;
    trainingProgress.totalSamples = status.totalSamples;
    trainingProgress.processedSamples = status.processedSamples;
    trainingProgress.currentAccuracy = status.currentAccuracy;
    trainingProgress.startTime = status.startTime;
    trainingProgress.currentPhase = status.currentPhase;
    
    Serial.println("[ML TRAINING] Training voltooid!");
    return true;
  }
  
  return false;
}

void mlTraining_stopTraining() {
  mlTrainer.stopTraining();
  trainingProgress.isTraining = false;
  Serial.println("[ML TRAINING] Training stopped");
}

MLTrainingProgress mlTraining_getProgress() {
  return trainingProgress;
}

bool mlTraining_saveModel(const String& modelName) {
  Serial.printf("[ML TRAINING] Saving model: %s\n", modelName.c_str());
  
  String filename = "/" + modelName;
  if (!filename.endsWith(".mlm")) {
    filename += ".mlm";
  }
  
  if (mlTrainer.saveModel(filename.c_str())) {
    Serial.println("[ML TRAINING] Model saved successfully");
    mlTraining_scanModels(); // Rescan models
    return true;
  }
  
  return false;
}

bool mlTraining_loadModel(const String& filename) {
  Serial.printf("[ML TRAINING] Loading model: %s\n", filename.c_str());
  
  String fullPath = "/" + filename;
  
  if (mlTrainer.loadModel(fullPath.c_str())) {
    Serial.println("[ML TRAINING] Model loaded successfully");
    return true;
  }
  
  return false;
}

// ===== Touch Handler Functions =====

MLTrainingEvent handleMainMenuTouch(int16_t tx, int16_t ty) {
  // Use exact same coordinates as drawing
  int16_t btnW = 200;
  int16_t btnH = 30;
  int16_t btnX = (320 - btnW) / 2;  // Gecentreerd
  int16_t startY = 55;
  int16_t gap = 8;
  
  // Check Train Model (.aly) button
  if (ty >= startY && ty < startY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Train Model pressed");
    return MTE_TRAIN_MODEL;
  }
  
  // Check AI Annotatie (.csv) button  
  int annotateY = startY + (btnH + gap);
  if (ty >= annotateY && ty < annotateY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] AI Annotatie pressed");
    return MTE_AI_ANNOTATE;
  }
  
  // Check Model Manager button
  int modelsY = startY + (btnH + gap) * 2;
  if (ty >= modelsY && ty < modelsY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Model Manager pressed");
    return MTE_MODEL_MANAGER;
  }
  
  // Check Training Info button
  int infoY = startY + (btnH + gap) * 3;
  if (ty >= infoY && ty < infoY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Training Info pressed");
    return MTE_TRAINING_INFO;
  }
  
  // Check TERUG button
  int backY = startY + (btnH + gap) * 4;
  if (ty >= backY && ty < backY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] TERUG pressed");
    return MTE_BACK;
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleImportTouch(int16_t tx, int16_t ty) {
  // File selection area (y: 50-180)
  if (ty >= 50 && ty < 180) {
    int fileIndex = (ty - 50) / 30; // Each file item is ~30 pixels high
    
    if (fileIndex >= 0 && fileIndex < fileCount) {
      if (selectedFileIndex == fileIndex) {
        // Double tap - import file
        Serial.printf("[ML TRAINING] Importing file: %s\n", fileList[fileIndex].filename.c_str());
        return MTE_IMPORT_FILE;
      } else {
        // Single tap - select file
        selectedFileIndex = fileIndex;
        mlTraining_drawImportScreen(); // Refresh to show selection
        Serial.printf("[ML TRAINING] Selected file: %s\n", fileList[fileIndex].filename.c_str());
      }
    }
    return MTE_NONE;
  }
  
  // Button area (y: 190+)
  if (ty >= 190) {
    if (tx >= 10 && tx < 90) { // Import button
      if (selectedFileIndex >= 0) {
        Serial.printf("[ML TRAINING] Import button pressed for: %s\n", fileList[selectedFileIndex].filename.c_str());
        return MTE_IMPORT_FILE;
      }
    } else if (tx >= 100 && tx < 180) { // Preview button
      if (selectedFileIndex >= 0) {
        mlTraining_setState(ML_STATE_PREVIEW);
      }
    } else if (tx >= 230 && tx < 310) { // Back button
      return MTE_BACK;
    }
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleTrainingTouch(int16_t tx, int16_t ty) {
  // Check TERUG button first (rechtsboven: SCR_W-75=245, y=10, w=65, h=25)
  if (ty >= 10 && ty < 35 && tx >= 245 && tx < 310) {
    Serial.println("[ML TRAINING] Training TERUG pressed");
    return MTE_BACK;
  }
  
  // Button area onderaan (y: 180+)
  if (ty >= 180) {
    if (trainingProgress.isTraining) {
      // Stop button
      if (tx >= 10 && tx < 110) {
        mlTraining_stopTraining();
        mlTraining_drawTrainingScreen(); // Refresh display
        return MTE_NONE;
      }
    } else if (trainingProgress.isComplete) {
      // Save and Discard buttons
      if (tx >= 10 && tx < 90) { // Save button
        Serial.println("[ML TRAINING] Save model pressed");
        return MTE_SAVE_MODEL;
      } else if (tx >= 100 && tx < 180) { // Discard button
        trainingProgress.isComplete = false;
        mlTraining_drawTrainingScreen(); // Refresh display
        return MTE_NONE;
      }
    } else {
      // Start button
      if (tx >= 10 && tx < 110) {
        Serial.println("[ML TRAINING] Start training pressed");
        return MTE_START_TRAINING;
      }
    }
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleModelManagerTouch(int16_t tx, int16_t ty) {
  // Model selection area (y: 50-160)
  if (ty >= 50 && ty < 160) {
    int modelIndex = (ty - 50) / 30; // Each model item is ~30 pixels high
    
    if (modelIndex >= 0 && modelIndex < modelCount) {
      selectedModelIndex = modelIndex;
      mlTraining_drawModelManager(); // Refresh to show selection
      Serial.printf("[ML TRAINING] Selected model: %s\n", modelList[modelIndex].name.c_str());
    }
    return MTE_NONE;
  }
  
  // Button area (y: 180+)
  if (ty >= 180) {
    if (tx >= 10 && tx < 70) { // Load button
      if (selectedModelIndex >= 0) {
        Serial.printf("[ML TRAINING] Load model pressed: %s\n", modelList[selectedModelIndex].name.c_str());
        return MTE_LOAD_MODEL;
      }
    } else if (tx >= 80 && tx < 140) { // Delete button
      if (selectedModelIndex >= 0 && !modelList[selectedModelIndex].isBuiltIn) {
        Serial.printf("[ML TRAINING] Delete model pressed: %s\n", modelList[selectedModelIndex].name.c_str());
        // TODO: Add confirmation dialog
        return MTE_NONE;
      }
    } else if (tx >= 150 && tx < 210) { // Info button
      if (selectedModelIndex >= 0) {
        mlTraining_setState(ML_STATE_MODEL_INFO);
      }
    } else if (tx >= 250 && tx < 310) { // Back button
      return MTE_BACK;
    }
  }
  
  return MTE_NONE;
}

// ===== AI Annotatie Drawing Functions =====

void mlTraining_drawAIAnnotateScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  const uint16_t COL_SELECT = 0x07E0; // Groen
  
  g->fillScreen(COL_BG);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("AI Annotatie", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("AI Annotatie");
  
  // Uitleg tekst
  g->setTextSize(1);
  g->setTextColor(COL_TX);
  int y = 50;
  
  g->setCursor(20, y);
  g->print("Selecteer .csv bestand om");
  y += 15;
  g->setCursor(20, y);
  g->print("AI voorspellingen te maken");
  y += 25;
  
  // Scan voor .csv bestanden
  File root = SD.open("/");
  int csvCount = 0;
  String csvFiles[5];
  
  if (root) {
    File file = root.openNextFile();
    while (file && csvCount < 5) {
      String filename = file.name();
      if (filename.endsWith(".csv")) {
        csvFiles[csvCount++] = filename;
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }
  
  if (csvCount == 0) {
    g->setCursor((SCR_W - 150) / 2, y + 20);
    g->print("Geen .csv bestanden");
  } else {
    // Toon .csv bestanden
    for (int i = 0; i < csvCount && y < 180; i++) {
      if (i == selectedFileIndex) {
        g->fillRoundRect(10, y - 2, SCR_W - 20, 20, 4, COL_SELECT);
        g->setTextColor(BLACK);
      } else {
        g->setTextColor(COL_TX);
      }
      
      g->setCursor(15, y);
      char shortName[30];
      strncpy(shortName, csvFiles[i].c_str(), 29);
      shortName[29] = '\0';
      g->print(shortName);
      
      y += 24;
    }
  }
  
  // Knoppen onderaan
  int btnY = SCR_H - 45;
  int btnW = 85;
  int btnH = 30;
  int spacing = 10;
  
  // Annotate knop - alleen actief als file geselecteerd en model geladen
  bool canAnnotate = (selectedFileIndex >= 0 && mlTrainer.hasModel());
  uint16_t annotateColor = canAnnotate ? 0xFFE0 : 0x4A49; // Geel of grijs
  drawMLButton(20, btnY, btnW, btnH, "Annoteer", annotateColor);
  
  // Back knop
  drawMLButton(SCR_W - btnW - 20, btnY, btnW, btnH, "Terug", 0x001F);
  
  // Status info
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  g->setCursor(10, 10);
  if (mlTrainer.hasModel()) {
    g->print("Model: OK");
  } else {
    g->setTextColor(0xF800); // Rood
    g->print("Geen model!");
  }
}

void mlTraining_drawAnnotateReviewScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = BLACK;
  const uint16_t COL_TX = WHITE;
  
  g->fillScreen(COL_BG);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("Review AI", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("Review AI");
  
  // Placeholder - dit zou sample data en voorspellingen tonen
  g->setTextSize(1);
  g->setTextColor(COL_TX);
  
  int y = 55;
  g->setCursor(20, y);
  g->print("AI voorspellingen gegenereerd!");
  
  y += 25;
  g->setCursor(20, y);
  g->print("Review functionaliteit");
  y += 15;
  g->setCursor(20, y);
  g->print("komt binnenkort...");
  
  y += 30;
  g->setCursor(20, y);
  g->print("Voor nu: voorspellingen worden");
  y += 15;
  g->setCursor(20, y);
  g->print("automatisch opgeslagen als .aly");
  
  // Knoppen onderaan
  int btnY = SCR_H - 45;
  int btnW = 85;
  int btnH = 30;
  
  drawMLButton((SCR_W - btnW) / 2, btnY, btnW, btnH, "OK", 0x07E0);
}

// ===== AI Annotatie Touch Handlers =====

MLTrainingEvent handleAIAnnotateTouch(int16_t tx, int16_t ty) {
  // File selection area (y: 90-180)
  if (ty >= 90 && ty < 180) {
    int fileIndex = (ty - 90) / 24;
    
    // Count .csv files to validate index
    File root = SD.open("/");
    int csvCount = 0;
    String csvFile = "";
    
    if (root) {
      File file = root.openNextFile();
      while (file && csvCount <= fileIndex) {
        String filename = file.name();
        if (filename.endsWith(".csv")) {
          if (csvCount == fileIndex) {
            csvFile = filename;
          }
          csvCount++;
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
    
    if (csvCount > fileIndex) {
      selectedFileIndex = fileIndex;
      mlTraining_drawAIAnnotateScreen(); // Refresh
      Serial.printf("[ML TRAINING] Selected .csv: %s\n", csvFile.c_str());
    }
    return MTE_NONE;
  }
  
  // Button area (y: 180+)
  int btnY = SCR_H - 45;
  int btnW = 85;
  
  if (ty >= btnY && ty < btnY + 30) {
    if (tx >= 20 && tx < 20 + btnW) { // Annoteer button
      if (selectedFileIndex >= 0 && mlTrainer.hasModel()) {
        Serial.println("[ML TRAINING] Starting AI annotation...");
        
        // Get selected .csv filename
        File root = SD.open("/");
        int csvCount = 0;
        String csvFile = "";
        
        if (root) {
          File file = root.openNextFile();
          while (file && csvCount <= selectedFileIndex) {
            String filename = file.name();
            if (filename.endsWith(".csv")) {
              if (csvCount == selectedFileIndex) {
                csvFile = filename;
              }
              csvCount++;
            }
            file.close();
            file = root.openNextFile();
          }
          root.close();
        }
        
        if (csvFile.length() > 0) {
          // Load CSV en genereer voorspellingen
          std::vector<TrainingSample> samples;
          String fullPath = "/" + csvFile;
          
          if (mlTrainer.loadCsvForAnnotation(fullPath.c_str(), samples)) {
            // Save als .aly
            String alyFile = csvFile;
            alyFile.replace(".csv", "_annotated.aly");
            String alyPath = "/" + alyFile;
            
            if (mlTrainer.savePredictions(alyPath.c_str(), samples)) {
              Serial.printf("[ML TRAINING] Saved predictions to: %s\n", alyFile.c_str());
              mlTraining_setState(ML_STATE_ANNOTATE_REVIEW);
              return MTE_NONE;
            }
          }
        }
      }
    } else if (tx >= SCR_W - btnW - 20 && tx < SCR_W - 20) { // Terug button
      return MTE_BACK;
    }
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleAnnotateReviewTouch(int16_t tx, int16_t ty) {
  // OK button area
  int btnY = SCR_H - 45;
  int btnW = 85;
  int btnX = (SCR_W - btnW) / 2;
  
  if (ty >= btnY && ty < btnY + 30 && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Review OK pressed");
    return MTE_BACK;
  }
  
  return MTE_NONE;
}

// ===== Eenvoudige Menu View (voor integratie in body_menu.cpp) =====

void drawMLTrainingView() {
  if (!body_gfx) return;
  
  // Menu layout (zelfde als andere submenu's)
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  const int BTN_W = MENU_W - 40;  // Volle breedte knoppen
  const int BTN_H = 38;           // Knop hoogte
  const int BTN_SPACING = 5;      // Verticale ruimte
  const int START_X = MENU_X + 20;
  const int START_Y = MENU_Y + 60;
  
  // Menu font voor knoppen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Check recording status
  extern bool isRecording;
  
  const char* items[] = {
    isRecording ? "REC STOP" : "REC START",
    "2. Model Trainen",
    "3. AI Annotatie",
    "4. Model Manager"
  };
  
  // Kleuren (zelfde als hoofdmenu)
  uint16_t colors[] = {
    isRecording ? 0xF800 : 0x07E0,  // Rood (STOP) of Groen (START)
    0xFFE0,  // Geel
    0xF81F,  // Magenta
    0x07FF   // Cyaan
  };
  
  // Teken 4 knoppen
  for (int i = 0; i < 4; i++) {
    int y = START_Y + i * (BTN_H + BTN_SPACING);
    
    // Gekleurde knop
    body_gfx->fillRoundRect(START_X, y, BTN_W, BTN_H, 8, colors[i]);
    
    // Highlight als geselecteerd
    extern int bodyMenuIdx;
    if (i == bodyMenuIdx) {
      body_gfx->drawRoundRect(START_X-2, y-2, BTN_W+4, BTN_H+4, 10, 0xFD20);  // Oranje
      body_gfx->drawRoundRect(START_X-1, y-1, BTN_W+2, BTN_H+2, 9, 0xFD20);
      body_gfx->drawRoundRect(START_X, y, BTN_W, BTN_H, 8, 0xFD20);
    } else {
      body_gfx->drawRoundRect(START_X, y, BTN_W, BTN_H, 8, 0xFFFF);
      body_gfx->drawRoundRect(START_X+1, y+1, BTN_W-2, BTN_H-2, 7, 0xFFFF);
    }

    // Zwarte tekst gecentreerd
    body_gfx->setTextColor(0x0000, colors[i]);  // Zwart op kleur
    int16_t x1, y1; uint16_t tw, th;
    body_gfx->getTextBounds(items[i], 0, 0, &x1, &y1, &tw, &th);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2 - x1, y + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2, y + (BTN_H - th) / 2 + th - 2);
    #endif
    body_gfx->print(items[i]);
  }
  
  // Status info onderaan (zoals op foto)
  body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_gfx->setTextSize(1);
  
  int statusY = START_Y + 4 * (BTN_H + BTN_SPACING) + 10;
  body_gfx->setCursor(START_X, statusY);
  body_gfx->printf("Files: %d  Models: %d", fileCount, modelCount);
  
  statusY += 15;
  body_gfx->setCursor(START_X, statusY);
  if (mlAnalyzer.hasModel()) {
    body_gfx->setTextColor(BODY_CFG.DOT_GREEN, BODY_CFG.COL_BG);
    body_gfx->print("Custom ML Model actief");
  } else {
    body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
    body_gfx->print("Rule-based actief");
  }
  
  // TERUG knop onderaan (blauw, zelfde stijl)
  int btnY = MENU_Y + 230;
  int btnW = MENU_W - 40;
  int btnX = MENU_X + 20;
  
  body_gfx->fillRoundRect(btnX, btnY, btnW, 40, 8, 0x001F);  // Blauw
  
  // Highlight TERUG als geselecteerd (index 4)
  extern int bodyMenuIdx;
  if (bodyMenuIdx == 4) {
    body_gfx->drawRoundRect(btnX-2, btnY-2, btnW+4, 44, 10, 0xFD20);  // Oranje
    body_gfx->drawRoundRect(btnX-1, btnY-1, btnW+2, 42, 9, 0xFD20);
    body_gfx->drawRoundRect(btnX, btnY, btnW, 40, 8, 0xFD20);
  } else {
    body_gfx->drawRoundRect(btnX, btnY, btnW, 40, 8, 0xFFFF);
    body_gfx->drawRoundRect(btnX+1, btnY+1, btnW-2, 38, 7, 0xFFFF);
  }
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  body_gfx->setTextColor(0xFFFF, 0x001F);  // Wit op blauw
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds("TERUG", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btnX + (btnW - tw) / 2 - x1, btnY + (40 + th) / 2 - 2);
  #else
    body_gfx->setCursor(btnX + (btnW - tw) / 2, btnY + (40 - th) / 2 + th - 2);
  #endif
  body_gfx->print("TERUG");
}
