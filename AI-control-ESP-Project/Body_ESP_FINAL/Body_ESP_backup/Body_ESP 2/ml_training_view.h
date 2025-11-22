/*
  ML Training View - Grafische interface voor Machine Learning training
  
  Provides touch-based interface for:
  - Importing .aly files (AI analyzed data)
  - Training ML models on imported data
  - Managing saved models (.mlm files)  
  - Visual training progress and statistics
*/

#ifndef ML_TRAINING_VIEW_H
#define ML_TRAINING_VIEW_H

// Simpele draw functie voor in het menu systeem
void drawMLTrainingView();

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ===== ML Training States =====
enum MLTrainingState {
  ML_STATE_MAIN,          // Main ML training menu
  ML_STATE_IMPORT,        // File import selection
  ML_STATE_PREVIEW,       // Preview selected file
  ML_STATE_TRAINING,      // Active training progress
  ML_STATE_MODEL_MANAGER, // Model management screen
  ML_STATE_MODEL_INFO,    // Model details view
  ML_STATE_AI_ANNOTATE,   // AI-assisted annotation (.csv -> .aly)
  ML_STATE_ANNOTATE_REVIEW // Review AI predictions
};

// ===== File Information Structure =====
struct MLFileInfo {
  String filename;        // e.g. "data001.aly"  
  String fullPath;        // Full path on SD card
  int sampleCount;        // Number of training samples
  int stressLevels[7];    // Count per stress level (1-7)
  float avgConfidence;    // Average confidence from AI Analyze
  uint32_t fileSize;      // File size in bytes
  String lastModified;    // Last modification date
  bool isValid;           // File format validation
  
  MLFileInfo() : sampleCount(0), avgConfidence(0.0f), fileSize(0), isValid(false) {
    for(int i = 0; i < 7; i++) stressLevels[i] = 0;
  }
};

// ===== Model Information Structure =====
struct MLModelInfo {
  String filename;        // e.g. "personal_v1.mlm"
  String name;           // Display name
  float accuracy;        // Model accuracy (0.0-1.0)
  int trainingSize;      // Number of training samples used
  String trainedDate;    // When model was created
  uint32_t modelSize;    // Model file size
  bool isActive;         // Currently loaded model
  bool isBuiltIn;        // Built-in rule-based model
  
  MLModelInfo() : accuracy(0.0f), trainingSize(0), modelSize(0), isActive(false), isBuiltIn(false) {}
};

// ===== Training Progress Structure =====
struct MLTrainingProgress {
  int totalSamples;       // Total samples to process
  int processedSamples;   // Samples processed so far
  float currentAccuracy;  // Current model accuracy
  uint32_t startTime;     // Training start time
  uint32_t estimatedTime; // Estimated completion time
  String currentPhase;    // "Preprocessing", "Training", "Validation"
  bool isTraining;        // Training in progress
  bool isComplete;        // Training completed
  bool hasError;          // Training error occurred
  String errorMessage;    // Error details if any
  
  MLTrainingProgress() : totalSamples(0), processedSamples(0), currentAccuracy(0.0f), 
                        startTime(0), estimatedTime(0), isTraining(false), isComplete(false), hasError(false) {}
};

// ===== Touch Events =====
enum MLTrainingEvent {
  MTE_NONE = 0,
  MTE_BACK,
  
  // Main menu
  MTE_IMPORT_DATA,
  MTE_TRAIN_MODEL,
  MTE_MODEL_MANAGER,
  MTE_TRAINING_INFO,
  
  // File import
  MTE_FILE_SELECT,
  MTE_FILE_PREVIEW,
  MTE_IMPORT_FILE,
  MTE_IMPORT_CONFIRM,
  MTE_IMPORT_CANCEL,
  
  // Training
  MTE_START_TRAINING,
  MTE_TRAINING_START,
  MTE_TRAINING_STOP,
  MTE_SAVE_MODEL,
  MTE_TRAINING_SAVE,
  MTE_TRAINING_DISCARD,
  
  // AI Annotation
  MTE_AI_ANNOTATE,
  MTE_ANNOTATE_ACCEPT,
  MTE_ANNOTATE_CORRECT,
  MTE_ANNOTATE_SAVE,
  
  // Model manager
  MTE_MODEL_SELECT,
  MTE_LOAD_MODEL,
  MTE_MODEL_LOAD,
  MTE_MODEL_DELETE,
  MTE_MODEL_EXPORT,
  MTE_MODEL_INFO,
  MTE_MODEL_RENAME
};

// ===== Global Functions =====

// Initialize ML training view system
bool mlTraining_begin(Arduino_GFX *gfx);

// Main event polling function
MLTrainingEvent mlTraining_poll();

// State management
void mlTraining_setState(MLTrainingState newState);
MLTrainingState mlTraining_getState();

// File operations  
bool mlTraining_scanAlyFiles();
int mlTraining_getFileCount();
MLFileInfo mlTraining_getFileInfo(int index);
bool mlTraining_importFile(const String& filename);

// Training operations
bool mlTraining_startTraining();
void mlTraining_stopTraining();
MLTrainingProgress mlTraining_getProgress();
bool mlTraining_saveModel(const String& modelName);

// Model management
bool mlTraining_scanModels();
int mlTraining_getModelCount();
MLModelInfo mlTraining_getModelInfo(int index);
bool mlTraining_loadModel(const String& filename);
bool mlTraining_deleteModel(const String& filename);
bool mlTraining_exportModel(const String& filename);

// UI Drawing functions
void mlTraining_drawMainMenu();
void mlTraining_drawImportScreen();
void mlTraining_drawPreviewScreen();
void mlTraining_drawTrainingScreen();
void mlTraining_drawModelManager();
void mlTraining_drawModelInfo();
void mlTraining_drawAIAnnotateScreen();
void mlTraining_drawAnnotateReviewScreen();

// Utility functions
String mlTraining_formatFileSize(uint32_t bytes);
String mlTraining_formatDuration(uint32_t seconds);
String mlTraining_formatAccuracy(float accuracy);

// Touch handling
bool mlTraining_handleTouch(int16_t x, int16_t y);
MLTrainingEvent handleMainMenuTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleImportTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleTrainingTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleModelManagerTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleAIAnnotateTouch(int16_t tx, int16_t ty);
MLTrainingEvent handleAnnotateReviewTouch(int16_t tx, int16_t ty);

// Global state variables
extern MLTrainingState currentMLState;
extern int selectedFileIndex;
extern int selectedModelIndex;
extern MLTrainingProgress trainingProgress;

// UI Constants
#define ML_MENU_ITEMS 4
#define ML_BUTTON_HEIGHT 40
#define ML_BUTTON_MARGIN 10
#define ML_TITLE_HEIGHT 30
#define ML_STATUS_HEIGHT 20

// Colors (using Body ESP color scheme)
#define ML_COLOR_BG        0x0000    // Black background
#define ML_COLOR_TEXT      0xFFFF    // White text
#define ML_COLOR_BUTTON    0x4208    // Dark blue button
#define ML_COLOR_SELECTED  0x07E0    // Green selection
#define ML_COLOR_ERROR     0xF800    // Red error
#define ML_COLOR_SUCCESS   0x07E0    // Green success  
#define ML_COLOR_WARNING   0xFFE0    // Yellow warning
#define ML_COLOR_PROGRESS  0x001F    // Blue progress bar

#endif // ML_TRAINING_VIEW_H