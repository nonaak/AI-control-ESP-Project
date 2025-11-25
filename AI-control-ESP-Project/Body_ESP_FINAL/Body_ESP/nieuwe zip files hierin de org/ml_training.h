/*
  ML TRAINING SYSTEM
  
  ═══════════════════════════════════════════════════════════════════════════
  LEERT VAN JOUW FEEDBACK - NIET ZWART-WIT!
  ═══════════════════════════════════════════════════════════════════════════
  
  Dit systeem logt elke nunchuk override MET sensor context zodat ML
  kan leren WAAROM je corrigeerde, niet alleen DAT je corrigeerde.
  
  Na genoeg training data kan ML op 100% autonomy werken en jou
  maximaal laten genieten!
*/

#ifndef ML_TRAINING_H
#define ML_TRAINING_H

#include <Arduino.h>
#include <SD_MMC.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         CONFIGURATIE
// ═══════════════════════════════════════════════════════════════════════════

#define ML_TRAINING_DIR         "/ml_training"
#define ML_FEEDBACK_FILE        "/ml_training/feedback.csv"
#define ML_MODEL_FILE           "/ml_training/model.bin"
#define ML_SESSION_PREFIX       "/ml_training/session_"

#define ML_MAX_TRAINING_SAMPLES 10000   // Max samples in geheugen
#define ML_FEATURE_COUNT        12      // Aantal input features
#define ML_HIDDEN_NEURONS       8       // Hidden layer grootte
#define ML_LEARNING_RATE        0.01f   // Learning rate voor training
#define ML_MOMENTUM             0.9f    // Momentum voor snellere convergentie

// ═══════════════════════════════════════════════════════════════════════════
//                    FEEDBACK SAMPLE STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct FeedbackSample {
  // Timestamp
  uint32_t timestamp;           // Millis sinds sessie start
  uint32_t sessionTime;         // Seconden in sessie
  
  // Sensor data op moment van feedback
  float heartRate;              // BPM
  float heartRateAvg;           // Gemiddelde HR laatste 30 sec
  float heartRateTrend;         // Stijgend (+) of dalend (-)
  
  float temperature;            // Huidige temp
  float tempDelta;              // Verschil met baseline
  
  float gsr;                    // Huidige GSR
  float gsrAvg;                 // Gemiddelde GSR laatste 30 sec
  float gsrTrend;               // Stijgend of dalend
  
  // Context
  int edgeCount;                // Aantal edges tot nu toe
  float timeSinceLastEdge;      // Seconden sinds laatste edge
  int currentLevel;             // Huidig actief level
  
  // AI beslissing vs Gebruiker correctie
  int aiChosenLevel;            // Wat AI had gekozen
  int userChosenLevel;          // Wat JIJ koos (via nunchuk)
  
  // Berekende delta (wat AI moet leren)
  int correction;               // userLevel - aiLevel (-7 tot +7)
  
  // Was dit een goede beslissing achteraf?
  bool wasGoodDecision;         // Wordt later ingevuld bij edge/orgasme
};

// ═══════════════════════════════════════════════════════════════════════════
//                    NEURAL NETWORK STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct NeuralNetwork {
  // Input -> Hidden weights [FEATURES x HIDDEN]
  float weightsIH[ML_FEATURE_COUNT][ML_HIDDEN_NEURONS];
  float biasH[ML_HIDDEN_NEURONS];
  
  // Hidden -> Output weights [HIDDEN x 8] (8 output levels)
  float weightsHO[ML_HIDDEN_NEURONS][8];
  float biasO[8];
  
  // Training statistics
  uint32_t trainingEpochs;
  float lastLoss;
  uint32_t totalSamples;
  
  // Model versie
  uint16_t version;
  uint32_t lastTrainedTime;
};

// ═══════════════════════════════════════════════════════════════════════════
//                    TRAINING STATE
// ═══════════════════════════════════════════════════════════════════════════

struct TrainingState {
  // Sessie info
  bool sessionActive;
  uint32_t sessionStartTime;
  String sessionFilename;
  
  // Huidige sensor context (voor feedback logging)
  float currentHR;
  float currentHRAvg;
  float currentHRTrend;
  float currentTemp;
  float currentTempDelta;
  float currentGSR;
  float currentGSRAvg;
  float currentGSRTrend;
  
  // Edge tracking
  int edgeCount;
  uint32_t lastEdgeTime;
  
  // Feedback buffer (in-memory voor huidige sessie)
  FeedbackSample* feedbackBuffer;
  int feedbackCount;
  int feedbackCapacity;
  
  // Training stats
  int totalCorrections;         // Totaal aantal keer gecorrigeerd
  int correctionsUp;            // Keren dat je omhoog corrigeerde
  int correctionsDown;          // Keren dat je omlaag corrigeerde
  float avgCorrection;          // Gemiddelde correctie grootte
};

// ═══════════════════════════════════════════════════════════════════════════
//                    ML TRAINER CLASS
// ═══════════════════════════════════════════════════════════════════════════

class MLTrainer {
public:
  MLTrainer();
  ~MLTrainer();
  
  // ─── Initialisatie ───
  bool begin();
  
  // ─── Sessie Management ───
  bool startSession();
  void endSession();
  bool isSessionActive() { return state.sessionActive; }
  
  // ─── Sensor Updates (elke loop) ───
  void updateSensorContext(float hr, float temp, float gsr);
  
  // ─── Feedback Logging (bij nunchuk override) ───
  void logFeedback(int aiChosenLevel, int userChosenLevel);
  
  // ─── Edge Events ───
  void logEdgeEvent();
  void logOrgasmeEvent();
  
  // ─── Training ───
  bool trainModel(int epochs = 100);
  bool saveModel();
  bool loadModel();
  bool hasTrainedModel() { return modelLoaded; }
  
  // ─── Inference (gebruik geleerd model) ───
  int predictOptimalLevel(float hr, float temp, float gsr);
  float getConfidence() { return lastConfidence; }
  
  // ─── Statistics ───
  int getTotalFeedbackCount();
  int getSessionFeedbackCount() { return state.feedbackCount; }
  float getModelAccuracy() { return modelAccuracy; }
  String getModelInfo();
  
  // ─── Debug ───
  void printStats();
  void printLastFeedback();
  
private:
  TrainingState state;
  NeuralNetwork network;
  bool modelLoaded;
  float lastConfidence;
  float modelAccuracy;
  
  // Rolling averages voor sensor context
  float hrBuffer[30];
  float gsrBuffer[30];
  int bufferIndex;
  bool bufferFull;
  
  // ─── Internal Functions ───
  void initNetwork();
  float sigmoid(float x);
  float sigmoidDerivative(float x);
  void forwardPass(float* inputs, float* hidden, float* outputs);
  void backpropagate(float* inputs, float* hidden, float* outputs, int targetLevel);
  
  float calculateLoss(float* outputs, int targetLevel);
  void normalizeInputs(FeedbackSample& sample, float* normalized);
  
  bool saveFeedbackToSD(const FeedbackSample& sample);
  bool loadFeedbackFromSD(const char* filename);
  
  void updateRollingAverages(float hr, float gsr);
  float calculateTrend(float* buffer, int count);
};

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBAL INSTANCE
// ═══════════════════════════════════════════════════════════════════════════

extern MLTrainer mlTrainer;

// ═══════════════════════════════════════════════════════════════════════════
//                    CONVENIENCE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Start ML training sessie
bool ml_startTrainingSession();

// Stop sessie en sla op
void ml_endTrainingSession();

// Update sensor context (roep elke loop aan)
void ml_updateSensors(float hr, float temp, float gsr);

// Log feedback bij nunchuk override
// Roep dit aan vanuit ESP-NOW handler wanneer gebruiker corrigeert!
void ml_logUserCorrection(int aiLevel, int userLevel);

// Log edge event (voor context)
void ml_logEdge();

// Log orgasme (einde sessie marker)
void ml_logOrgasme();

// Train model op verzamelde data
bool ml_trainModel(int epochs = 100);

// Krijg ML voorspelling (gebruik NA training)
int ml_predictLevel(float hr, float temp, float gsr);

// Check of model getraind is
bool ml_hasTrainedModel();

// Print training stats
void ml_printTrainingStats();

#endif // ML_TRAINING_H
