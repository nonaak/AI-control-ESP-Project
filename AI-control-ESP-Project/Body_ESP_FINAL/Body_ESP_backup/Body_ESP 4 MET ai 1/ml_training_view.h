/*
  ML Training View Header
  
  Complete graphical interface for ML model training workflow
*/

#ifndef ML_TRAINING_VIEW_H
#define ML_TRAINING_VIEW_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Forward declaration
extern Arduino_GFX* body_gfx;

// Screen dimensions - gebruik extern van body_display.h
extern const int SCR_W;  // 480
extern const int SCR_H;  // 320

// ===== Colors =====
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define ORANGE  0xFD20
#define GREY    0x7BEF

// ===== State Machine =====
enum MLTrainingState {
  ML_STATE_MAIN,           // Hoofdmenu met 4 opties
  ML_STATE_IMPORT,         // Selecteer .aly bestand
  ML_STATE_PREVIEW,        // Bekijk bestand info
  ML_STATE_TRAINING,       // Training in progress
  ML_STATE_MODEL_MANAGER,  // Beheer modellen
  ML_STATE_MODEL_INFO,     // Model details
  ML_STATE_AI_ANNOTATE,    // AI Annotatie scherm
  ML_STATE_ANNOTATE_REVIEW // Review annotaties
};

// ===== Events =====
enum MLTrainingEvent {
  MTE_NONE,                // Geen event
  MTE_BACK,                // Terug knop
  MTE_IMPORT,              // Import geselecteerd
  MTE_IMPORT_FILE,         // Bestand import
  MTE_TRAIN,               // Start training
  MTE_TRAIN_MODEL,         // Train model
  MTE_START_TRAINING,      // Start training process
  MTE_SAVE_MODEL,          // Save trained model
  MTE_LOAD_MODEL,          // Load model
  MTE_MODEL_SELECT,        // Model geselecteerd
  MTE_MODEL_MANAGER,       // Open model manager
  MTE_FILE_SELECT,         // Bestand geselecteerd
  MTE_ANNOTATE,            // Start annotatie
  MTE_AI_ANNOTATE,         // AI annotatie
  MTE_REVIEW,              // Review annotaties
  MTE_DELETE,              // Verwijder model
  MTE_ACTIVATE,            // Activeer model
  MTE_TRAINING_INFO        // Training info
};

// ===== File Info Structure =====
struct MLFileInfo {
  String filename;         // Bestandsnaam
  String fullPath;         // Volledig pad
  int fileSize;            // Grootte in bytes
  int sampleCount;         // Aantal samples
  float avgConfidence;     // Gemiddelde confidence
  int stressLevels[8];     // Verdeling per level (0-7)
  bool isValid;            // Geldig bestand?
  
  // Constructor met defaults
  MLFileInfo() : fileSize(0), sampleCount(0), avgConfidence(0), isValid(false) {
    memset(stressLevels, 0, sizeof(stressLevels));
  }
};

// ===== Model Info Structure =====
struct MLModelInfo {
  String filename;         // Bestandsnaam (.mlm)
  String name;             // Weergavenaam
  float accuracy;          // Model nauwkeurigheid (0.0-1.0)
  int trainingSize;        // Aantal training samples
  int modelSize;           // Bestandsgrootte in bytes
  String trainedDate;      // Training datum
  bool isBuiltIn;          // Ingebouwd model?
  bool isActive;           // Momenteel actief?
  
  // Constructor met defaults
  MLModelInfo() : accuracy(0), trainingSize(0), modelSize(0), isBuiltIn(false), isActive(false) {}
};

// ===== Training Progress Structure =====
struct MLTrainingProgress {
  bool isTraining;         // Training bezig?
  bool isComplete;         // Training voltooid?
  int currentEpoch;        // Huidige epoch
  int totalEpochs;         // Totaal epochs
  int processedSamples;    // Verwerkte samples
  int totalSamples;        // Totaal samples
  float currentLoss;       // Huidige loss
  float currentAccuracy;   // Huidige accuracy
  float progress;          // Voortgang 0.0-1.0
  uint32_t startTime;      // Start timestamp (millis)
  String currentPhase;     // Huidige fase beschrijving
  bool hasError;           // Error opgetreden?
  String errorMessage;     // Error bericht
  
  // Constructor met defaults
  MLTrainingProgress() : isTraining(false), isComplete(false), currentEpoch(0), totalEpochs(0),
                         processedSamples(0), totalSamples(0), currentLoss(0), currentAccuracy(0), 
                         progress(0), startTime(0), hasError(false) {}
};

// ===== Global State Variables (extern) =====
extern MLTrainingState currentMLState;
extern int selectedFileIndex;
extern int selectedModelIndex;
extern MLTrainingProgress trainingProgress;

// ===== Initialization =====
bool mlTraining_begin(Arduino_GFX *gfx);

// ===== Event Polling =====
MLTrainingEvent mlTraining_poll();

// ===== State Management =====
void mlTraining_setState(MLTrainingState newState);
MLTrainingState mlTraining_getState();

// ===== File Operations =====
bool mlTraining_scanAlyFiles();
int mlTraining_getFileCount();
MLFileInfo mlTraining_getFileInfo(int index);

// ===== Model Operations =====
bool mlTraining_scanModels();
int mlTraining_getModelCount();
MLModelInfo mlTraining_getModelInfo(int index);
bool mlTraining_activateModel(int index);
bool mlTraining_deleteModel(int index);

// ===== Training Control =====
bool mlTraining_startTraining();
void mlTraining_stopTraining();
MLTrainingProgress mlTraining_getProgress();

// ===== Screen Drawing Functions =====
void mlTraining_drawMainMenu();
void mlTraining_drawImportScreen();
void mlTraining_drawPreviewScreen();
void mlTraining_drawTrainingScreen();
void mlTraining_drawModelManager();
void mlTraining_drawModelInfo();
void mlTraining_drawAIAnnotateScreen();
void mlTraining_drawAnnotateReviewScreen();

// ===== Touch Handlers (internal) =====
MLTrainingEvent handleMainMenuTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleImportTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleTrainingTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleModelManagerTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleAIAnnotateTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleAnnotateReviewTouch(int16_t tx, int16_t ty);

// ===== UI Helper =====
void drawMLButton(int x, int y, int w, int h, const char* label, uint16_t color);

// ===== Simple Menu View (voor body_menu.cpp integratie) =====
void drawMLTrainingView();

#endif // ML_TRAINING_VIEW_H
