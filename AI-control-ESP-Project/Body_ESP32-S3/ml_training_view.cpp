/*
  ML Training View Implementation
  
  Complete graphical interface for ML model training workflow
  FIXED: NULL pointer crash when SD card not present
  FIXED: Double SD_MMC.begin() crash
*/

#include "ml_training_view.h"
#include "ml_stress_analyzer.h"
#include "input_touch.h"
#include <SD_MMC.h>
#include <FS.h>
#include "body_config.h"

// External function declarations (defined in Body_ESP.ino)
extern void startRecording();
extern void stopRecording();
extern bool isRecording;

// ===== Global Variables =====
static TFT_eSPI* g = nullptr;
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

// SD card availability flag
static bool sdCardAvailable = false;

// Touch state
static bool touchPressed = false;
static uint32_t lastTouchTime = 0;
static const uint32_t TOUCH_DEBOUNCE = 200; // 200ms debounce

// ===== Initialization =====

bool mlTraining_begin(TFT_eSPI *gfx) {
  g = gfx;
  
  // CRITICAL: Check if g is valid
  if (!g) {
    Serial.println("[ML TRAINING] ERROR: Invalid TFT_eSPI pointer!");
    return false;
  }
  
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
  
  // SD is already initialized in setup(), just check if available
  // CRITICAL: Don't call SD_MMC.begin() again - that causes crashes!
  sdCardAvailable = (SD_MMC.cardType() != CARD_NONE);
  
  if (!sdCardAvailable) {
    Serial.println("[ML TRAINING] Warning: SD card not available - limited functionality");
    fileCount = 0;
    modelCount = 0;
  } else {
    // Scan for .aly files and models
    if (!mlTraining_scanAlyFiles()) {
      Serial.println("[ML TRAINING] Warning: No .aly files found");
      fileCount = 0;
    }
    if (!mlTraining_scanModels()) {
      Serial.println("[ML TRAINING] Warning: Model scan failed");
      modelCount = 1;
    }
  }
  
  // Always ensure at least the built-in model exists
  if (modelCount == 0) {
    Serial.println("[ML TRAINING] Creating default built-in model entry");
    MLModelInfo &builtIn = modelList[modelCount++];
    builtIn.filename = "rule_based";
    builtIn.name = "Rule-based";
    builtIn.accuracy = 0.80f;
    builtIn.trainingSize = 0;
    builtIn.trainedDate = "Built-in";
    builtIn.isBuiltIn = true;
    builtIn.isActive = true;
  }
  
  mlTraining_drawMainMenu();
  
  Serial.println("[ML TRAINING] Initialization complete");
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
    default:
      // For other states, just handle back button
      if (ty > 190 && tx > 230 && tx < 310) {
        return MTE_BACK;
      }
      break;
  }
  
  return MTE_NONE;
}

// ===== State Management =====

void mlTraining_setState(MLTrainingState newState) {
  if (currentMLState != newState) {
    Serial.print("[ML TRAINING] State change: ");
    Serial.print(currentMLState);
    Serial.print(" -> ");
    Serial.println(newState);
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
  
  // Check SD card availability
  if (!sdCardAvailable) {
    Serial.println("[ML TRAINING] SD card not available for scanning");
    return false;
  }
  
  fs::File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("[ML TRAINING] Cannot open root directory");
    return false;
  }
  
  fs::File file = root.openNextFile();
  while (file && fileCount < 10) {
    String filename = file.name();
    
    if (filename.endsWith(".aly")) {
      MLFileInfo &info = fileList[fileCount];
      info.filename = filename;
      info.fullPath = "/" + filename;
      info.fileSize = file.size();
      info.isValid = true;
      
      // Parse file to get sample count and stats (with safety checks)
      if (!mlTraining_parseAlyFile(info)) {
        Serial.print("[ML TRAINING] Warning: Failed to parse ");
        Serial.println(filename);
        info.isValid = false;
      }
      
      fileCount++;
      Serial.print("[ML TRAINING] Found .aly file: ");
      Serial.print(filename);
      Serial.print(" (");
      Serial.print(info.fileSize);
      Serial.println(" bytes)");
    }
    
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return (fileCount > 0);
}

bool mlTraining_parseAlyFile(MLFileInfo &info) {
  // Safety check: SD card must be available
  if (!sdCardAvailable) {
    Serial.println("[ML TRAINING] Cannot parse file: SD card unavailable");
    info.isValid = false;
    return false;
  }
  
  // Safety check: fullPath must not be empty
  if (info.fullPath.length() == 0) {
    Serial.println("[ML TRAINING] Cannot parse file: empty path");
    info.isValid = false;
    return false;
  }
  
  fs::File file = SD_MMC.open(info.fullPath);
  if (!file) {
    Serial.print("[ML TRAINING] Cannot open file: ");
    Serial.println(info.fullPath);
    info.isValid = false;
    return false;
  }
  
  String line;
  int samples = 0;
  float confidenceSum = 0.0f;
  
  // Initialize stress levels array
  for (int i = 0; i < 7; i++) {
    info.stressLevels[i] = 0;
  }
  
  // Skip header line
  if (file.available()) {
    line = file.readStringUntil('\n');
  }
  
  // Parse data lines
  while (file.available() && samples < 1000) {
    line = file.readStringUntil('\n');
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
  return MLFileInfo();
}

// ===== Model Management =====

bool mlTraining_scanModels() {
  modelCount = 0;
  
  // Add built-in rule-based model (always available)
  MLModelInfo &builtIn = modelList[modelCount++];
  builtIn.filename = "rule_based";
  builtIn.name = "Rule-based Classifier";
  builtIn.accuracy = 0.80f;
  builtIn.trainingSize = 0;
  builtIn.trainedDate = "Built-in";
  builtIn.isBuiltIn = true;
  builtIn.isActive = true;
  
  Serial.println("[ML TRAINING] Built-in model added");
  
  // Only scan for .mlm files if SD card is available
  if (!sdCardAvailable) {
    Serial.println("[ML TRAINING] Skipping model scan: SD card unavailable");
    return true;
  }
  
  fs::File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("[ML TRAINING] Cannot open root for model scan");
    return true;
  }
  
  fs::File file = root.openNextFile();
  while (file && modelCount < 5) {
    String filename = file.name();
    
    if (filename.endsWith(".mlm")) {
      MLModelInfo &info = modelList[modelCount];
      info.filename = filename;
      info.name = filename.substring(0, filename.length() - 4);
      info.modelSize = file.size();
      info.isBuiltIn = false;
      info.isActive = false;
      
      info.accuracy = 0.85f;
      info.trainingSize = 500;
      info.trainedDate = "Unknown";
      
      modelCount++;
      Serial.print("[ML TRAINING] Found model: ");
      Serial.print(filename);
      Serial.print(" (");
      Serial.print(info.modelSize);
      Serial.println(" bytes)");
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
  return MLModelInfo();
}

// ===== Drawing Functions =====

static void drawMLButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* txt, uint16_t color) {
  if (!g) return;
  
  const uint16_t COL_BG = TFT_BLACK;
  const uint16_t COL_FR = 0xC618;
  
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(TFT_BLACK);
  g->setTextSize(1);
  
  uint16_t tw = strlen(txt) * 6;
  uint16_t th = 8;
  int textX = x + (w - tw)/2;
  int textY = y + (h - th)/2;
  g->setCursor(textX, textY);
  g->print(txt);
}

void mlTraining_drawMainMenu() {
  if (!g) {
    Serial.println("[ML TRAINING] ERROR: Cannot draw - invalid display pointer!");
    return;
  }
  
  const uint16_t COL_BG = TFT_BLACK;
  const uint16_t COL_TX = TFT_WHITE;
  
  int16_t SCR_W = g->width();
  int16_t SCR_H = g->height();
  
  Serial.println("[ML TRAINING] Drawing main menu...");
  
  g->fillScreen(COL_BG);
  
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  uint16_t tw = strlen("ML Training") * 12;
  g->setCursor((SCR_W - tw) / 2, 25);
  g->print("ML Training");
  
  if (!sdCardAvailable) {
    g->setTextSize(1);
    g->setTextColor(0xF800);
    tw = strlen("SD KAART NIET GEVONDEN!") * 6;
    g->setCursor((SCR_W - tw) / 2, 45);
    g->print("SD KAART NIET GEVONDEN!");
    g->setTextColor(COL_TX);
  }
  
  int16_t btnW = 200;
  int16_t btnH = 30;
  int16_t btnX = (SCR_W - btnW) / 2;
  int16_t startY = sdCardAvailable ? 55 : 65;
  int16_t gap = 8;
  
  const uint16_t COL_IMPORT = 0x07E0;
  const uint16_t COL_TRAIN = 0xFFE0;
  const uint16_t COL_MODELS = 0xF81F;
  const uint16_t COL_INFO = 0x07FF;
  const uint16_t COL_BACK = 0x001F;
  
  const char* menuItems[] = {
    "Import Data",
    "Train Model", 
    "Model Manager",
    "Training Info"
  };
  
  Serial.println("[ML TRAINING] Drawing buttons...");
  
  drawMLButton(btnX, startY, btnW, btnH, menuItems[0], COL_IMPORT);
  drawMLButton(btnX, startY + (btnH + gap), btnW, btnH, menuItems[1], COL_TRAIN);
  drawMLButton(btnX, startY + (btnH + gap) * 2, btnW, btnH, menuItems[2], COL_MODELS);
  drawMLButton(btnX, startY + (btnH + gap) * 3, btnW, btnH, menuItems[3], COL_INFO);
  drawMLButton(btnX, startY + (btnH + gap) * 4, btnW, btnH, "TERUG", COL_BACK);
  
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  
  g->setCursor(20, SCR_H - 30);
  g->print("Files: ");
  g->print(fileCount);
  g->print("  Models: ");
  g->print(modelCount);
  
  g->setCursor(20, SCR_H - 15);
  g->print("Rule-based active");
  
  Serial.println("[ML TRAINING] Main menu drawn successfully");
}

void mlTraining_drawImportScreen() {
  if (!g) return;
  
  const uint16_t COL_BG = TFT_BLACK;
  const uint16_t COL_TX = TFT_WHITE;
  const uint16_t COL_SELECT = 0x07E0;
  
  int16_t SCR_W = g->width();
  int16_t SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  uint16_t tw = strlen("Import Data") * 12;
  g->setCursor((SCR_W - tw) / 2, 20);
  g->print("Import Data");
  
  if (!sdCardAvailable) {
    g->setTextSize(1);
    g->setTextColor(0xF800);
    tw = strlen("SD kaart niet beschikbaar") * 6;
    g->setCursor((SCR_W - tw) / 2, 80);
    g->print("SD kaart niet beschikbaar");
    
    drawMLButton(SCR_W - 85, SCR_H - 45, 75, 30, "Terug", 0x001F);
    return;
  }
  
  int y = 50;
  
  if (fileCount == 0) {
    g->setTextSize(1);
    g->setTextColor(COL_TX);
    g->setCursor((SCR_W - 120) / 2, y + 30);
    g->print("Geen .aly bestanden");
    g->setCursor((SCR_W - 180) / 2, y + 50);
    g->print("Gebruik AI Analyze eerst");
  } else {
    g->setTextSize(1);
    for (int i = 0; i < fileCount && y < (SCR_H - 80); i++) {
      MLFileInfo info = fileList[i];
      
      if (i == selectedFileIndex) {
        g->fillRoundRect(10, y - 2, SCR_W - 20, 28, 4, COL_SELECT);
        g->setTextColor(TFT_BLACK);
      } else {
        g->setTextColor(COL_TX);
      }
      
      g->setCursor(15, y + 2);
      char shortName[25];
      strncpy(shortName, info.filename.c_str(), 24);
      shortName[24] = '\0';
      g->print(shortName);
      
      char sampleText[15];
      snprintf(sampleText, sizeof(sampleText), "%d smp", info.sampleCount);
      tw = strlen(sampleText) * 6;
      g->setCursor(SCR_W - tw - 15, y + 2);
      g->print(sampleText);
      
      g->setCursor(15, y + 14);
      g->print(info.fileSize / 1024.0f, 1);
      g->print("kB");
      
      char confText[12];
      float confPercent = info.avgConfidence * 100.0f;
      snprintf(confText, sizeof(confText), "%.0f", confPercent);
      tw = strlen(confText) * 6 + 6;
      g->setCursor(SCR_W - tw - 15, y + 14);
      g->print(confText);
      g->print("%");
      
      y += 32;
    }
  }
  
  int btnY = SCR_H - 50;
  int btnW = 75;
  int btnH = 30;
  int spacing = 10;
  
  uint16_t importColor = (selectedFileIndex >= 0) ? COL_SELECT : 0x4A49;
  drawMLButton(10, btnY, btnW, btnH, "Import", importColor);
  drawMLButton(10 + btnW + spacing, btnY, btnW, btnH, "Preview", 0xFFE0);
  drawMLButton(SCR_W - btnW - 10, btnY, btnW, btnH, "Terug", 0x001F);
  
  if (fileCount > 0) {
    g->setTextColor(COL_TX);
    g->setTextSize(1);
    g->setCursor(10, 10);
    g->print(fileCount);
    g->print(" bestanden");
  }
}

void mlTraining_drawTrainingScreen() {
  if (!g) return;
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_WHITE);
  g->setTextSize(2);
  g->setCursor(60, 100);
  g->print("Training Screen");
  drawMLButton(120, 170, 80, 30, "Terug", 0x001F);
}

void mlTraining_drawModelManager() {
  if (!g) return;
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_WHITE);
  g->setTextSize(2);
  g->setCursor(40, 100);
  g->print("Model Manager");
  drawMLButton(120, 170, 80, 30, "Terug", 0x001F);
}

void mlTraining_drawPreviewScreen() {
  if (!g) return;
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_WHITE);
  g->setTextSize(2);
  g->setCursor(70, 100);
  g->print("Preview");
  drawMLButton(120, 170, 80, 30, "Terug", 0x001F);
}

void mlTraining_drawModelInfo() {
  if (!g) return;
  g->fillScreen(TFT_BLACK);
  g->setTextColor(TFT_WHITE);
  g->setTextSize(2);
  g->setCursor(60, 100);
  g->print("Model Info");
  drawMLButton(120, 170, 80, 30, "Terug", 0x001F);
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

// ===== Training Operations =====

bool mlTraining_startTraining() {
  trainingProgress.isTraining = true;
  trainingProgress.isComplete = false;
  trainingProgress.hasError = false;
  trainingProgress.processedSamples = 0;
  trainingProgress.totalSamples = 100;
  trainingProgress.currentAccuracy = 0.0f;
  trainingProgress.startTime = millis();
  trainingProgress.currentPhase = "Initializing";
  
  Serial.println("[ML TRAINING] Training started");
  return true;
}

void mlTraining_stopTraining() {
  trainingProgress.isTraining = false;
  Serial.println("[ML TRAINING] Training stopped");
}

MLTrainingProgress mlTraining_getProgress() {
  return trainingProgress;
}

bool mlTraining_saveModel(const String& modelName) {
  Serial.print("[ML TRAINING] Saving model: ");
  Serial.println(modelName);
  return true;
}

bool mlTraining_loadModel(const String& filename) {
  Serial.print("[ML TRAINING] Loading model: ");
  Serial.println(filename);
  return true;
}

// ===== Touch Handler Functions =====

MLTrainingEvent handleMainMenuTouch(int16_t tx, int16_t ty) {
  int16_t btnW = 200;
  int16_t btnH = 30;
  int16_t btnX = (320 - btnW) / 2;
  int16_t startY = sdCardAvailable ? 55 : 65;
  int16_t gap = 8;
  
  if (ty >= startY && ty < startY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Import Data pressed");
    return MTE_IMPORT_DATA;
  }
  
  int trainY = startY + (btnH + gap);
  if (ty >= trainY && ty < trainY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Train Model pressed");
    return MTE_TRAIN_MODEL;
  }
  
  int modelsY = startY + (btnH + gap) * 2;
  if (ty >= modelsY && ty < modelsY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Model Manager pressed");
    return MTE_MODEL_MANAGER;
  }
  
  int infoY = startY + (btnH + gap) * 3;
  if (ty >= infoY && ty < infoY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] Training Info pressed");
    return MTE_TRAINING_INFO;
  }
  
  int backY = startY + (btnH + gap) * 4;
  if (ty >= backY && ty < backY + btnH && tx >= btnX && tx < btnX + btnW) {
    Serial.println("[ML TRAINING] TERUG pressed");
    return MTE_BACK;
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleImportTouch(int16_t tx, int16_t ty) {
  if (!sdCardAvailable) {
    if (ty >= 195 && tx >= 235 && tx < 310) {
      return MTE_BACK;
    }
    return MTE_NONE;
  }
  
  if (ty >= 50 && ty < 180) {
    int fileIndex = (ty - 50) / 32;
    
    if (fileIndex >= 0 && fileIndex < fileCount) {
      if (selectedFileIndex == fileIndex) {
        Serial.print("[ML TRAINING] Importing file: ");
        Serial.println(fileList[fileIndex].filename);
        return MTE_IMPORT_FILE;
      } else {
        selectedFileIndex = fileIndex;
        mlTraining_drawImportScreen();
        Serial.print("[ML TRAINING] Selected file: ");
        Serial.println(fileList[fileIndex].filename);
      }
    }
    return MTE_NONE;
  }
  
  if (ty >= 190) {
    if (tx >= 10 && tx < 90) {
      if (selectedFileIndex >= 0) {
        Serial.print("[ML TRAINING] Import button pressed for: ");
        Serial.println(fileList[selectedFileIndex].filename);
        return MTE_IMPORT_FILE;
      }
    } else if (tx >= 100 && tx < 180) {
      if (selectedFileIndex >= 0) {
        mlTraining_setState(ML_STATE_PREVIEW);
      }
    } else if (tx >= 230 && tx < 310) {
      return MTE_BACK;
    }
  }
  
  return MTE_NONE;
}

MLTrainingEvent handleTrainingTouch(int16_t tx, int16_t ty) {
  if (ty >= 170 && tx >= 120 && tx < 200) {
    return MTE_BACK;
  }
  return MTE_NONE;
}

MLTrainingEvent handleModelManagerTouch(int16_t tx, int16_t ty) {
  if (ty >= 170 && tx >= 120 && tx < 200) {
    return MTE_BACK;
  }
  return MTE_NONE;
}