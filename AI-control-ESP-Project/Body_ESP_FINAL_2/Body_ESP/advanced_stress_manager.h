/*
  Advanced Stress Management System - Body ESP
  
  Intelligent 7-level stress management with:
  - Configurable timing per stress level  
  - ML integration for learning personal patterns
  - Stress change detection (rustig/snel/heel snel)
  - Enhanced CSV logging for ML training
  - Vibe/Suction control based on stress reactions
*/

#ifndef ADVANCED_STRESS_MANAGER_H
#define ADVANCED_STRESS_MANAGER_H

#include <Arduino.h>
#include "body_config.h"

// ===== Stress Level Definitions =====
enum StressLevel : uint8_t {
  STRESS_0_NORMAAL = 0,      // Normaal - geen stress, versnelling 1
  STRESS_1_GEEN = 1,         // Geen/beetje stress, versnelling 2  
  STRESS_2_BEETJE = 2,       // Beetje stress, versnelling 3
  STRESS_3_IETS_MEER = 3,    // Iets meer stress, versnelling 4
  STRESS_4_GEMIDDELD = 4,    // Gemiddeld stress, versnelling 4-6 (reactief)
  STRESS_5_MEER = 5,         // Meer stress, versnelling 5-7 (reactief)  
  STRESS_6_VEEL = 6,         // Veel stress, versnelling 6-7 (reactief)
  STRESS_7_MAX = 7           // Maximum stress, alles MAX
};

// ===== Stress Change Types =====
enum StressChangeType : uint8_t {
  CHANGE_NONE = 0,           // Geen significante verandering
  CHANGE_RUSTIG_OMHOOG = 1,  // Rustige stijging
  CHANGE_RUSTIG_OMLAAG = 2,  // Rustige daling
  CHANGE_NORMAAL_OMHOOG = 3, // Normale stijging  
  CHANGE_NORMAAL_OMLAAG = 4, // Normale daling
  CHANGE_SNEL_OMHOOG = 5,    // Snelle stijging
  CHANGE_SNEL_OMLAAG = 6,    // Snelle daling
  CHANGE_HEEL_SNEL_OMHOOG = 7, // Heel snelle stijging
  CHANGE_HEEL_SNEL_OMLAAG = 8  // Heel snelle daling
};

// ===== Stress Management Actions =====
enum StressAction : uint8_t {
  ACTION_NONE = 0,
  ACTION_SPEED_UP = 1,       // Versnelling verhogen
  ACTION_SPEED_DOWN = 2,     // Versnelling verlagen  
  ACTION_VIBE_ON = 3,        // Vibe aanzetten
  ACTION_VIBE_OFF = 4,       // Vibe uitzetten
  ACTION_SUCTION_ON = 5,     // Zuigen aanzetten
  ACTION_SUCTION_OFF = 6,    // Zuigen uitzetten
  ACTION_MAX_MODE = 7,       // Alles naar maximum
  ACTION_EMERGENCY_STOP = 8, // Noodstop (C-knop)
  ACTION_WAIT = 9,           // Wachten (timer niet verlopen)
  // ML Autonomy actions
  ACTION_ML_OVERRIDE = 10,   // ML override van regels
  ACTION_ML_SKIP_LEVEL = 11, // ML slaat level over
  ACTION_ML_CUSTOM = 12      // ML custom beslissing
};

// ===== Biometric Data Structure =====
struct BiometricData {
  float heartRate = 0.0f;
  float temperature = 0.0f;
  float gsrValue = 0.0f;
  uint32_t timestamp = 0;
  
  BiometricData() {}
  BiometricData(float hr, float temp, float gsr) 
    : heartRate(hr), temperature(temp), gsrValue(gsr), timestamp(millis()) {}
};

// ===== Stress Decision Structure =====
struct StressDecision {
  StressLevel currentLevel = STRESS_0_NORMAAL;
  StressLevel previousLevel = STRESS_0_NORMAAL;
  StressChangeType changeType = CHANGE_NONE;
  StressAction recommendedAction = ACTION_NONE;
  uint8_t recommendedSpeed = 1;
  bool vibeRecommended = false;
  bool suctionRecommended = false;
  float confidence = 0.0f;
  String reasoning = "";
  uint32_t timestamp = 0;
  bool isMLPrediction = false;
  
  // ML Autonomy fields
  bool isMLOverride = false;     // ML heeft regels overschreven
  float mlAutonomyUsed = 0.0f;   // Percentage autonomie gebruikt (0.0-1.0)
  String mlReasoning = "";       // ML-specifieke redenering
  
  StressDecision() { timestamp = millis(); }
};

// ===== ML Training Data Structure =====
struct MLTrainingData {
  BiometricData biometrics;
  StressDecision decision;
  StressAction actionTaken = ACTION_NONE;
  uint8_t actualSpeed = 1;
  bool actualVibe = false;
  bool actualSuction = false;
  String userResponse = "";      // "CONTINUE", "KLAAR!", "STRESS_HIGH", etc.
  uint32_t sessionDurationMs = 0;
  bool isSessionEnd = false;
  
  MLTrainingData() {}
};

// ===== Advanced Stress Manager Class =====
class AdvancedStressManager {
private:
  // Current state
  StressLevel currentStressLevel = STRESS_0_NORMAAL;
  StressLevel previousStressLevel = STRESS_0_NORMAAL;
  uint32_t levelStartTime = 0;
  uint32_t sessionStartTime = 0;
  bool sessionActive = false;
  
  // Biometric history for trend analysis
  BiometricData biometricHistory[10];  // Rolling buffer van laatste 10 metingen
  uint8_t historyIndex = 0;
  uint8_t historyCount = 0;
  
  // Stress change detection
  float lastStressValue = 0.0f;
  uint32_t lastStressTime = 0;
  
  // ML integration
  bool mlEnabled = false;
  uint32_t lastMLUpdate = 0;
  StressChangeType lastStressChange = CHANGE_NONE;
  
  // ML Autonomy tracking
  uint32_t totalSessions = 0;
  float currentAutonomyLevel = 0.0f;
  bool mlAutonomyActive = false;
  
  // Internal methods
  float calculateBiometricStress(const BiometricData& data);
  StressChangeType detectStressChange(float currentStress);
  StressDecision makeRuleBasedDecision();
  StressDecision makeReactiveDecision();
  StressDecision makeMLDecision();
  StressDecision makeHybridDecision();  // Nieuwe hybride beslissingslogica
  void updateBiometricHistory(const BiometricData& data);
  bool isTimerExpired();
  uint32_t getCurrentLevelTimeout();
  void updateMLAutonomyStatus();  // Update ML autonomy based on config and sessions
  
public:
  // Constructor
  AdvancedStressManager();
  
  // Main interface
  void begin();
  void update(const BiometricData& biometrics);
  StressDecision getStressDecision();
  void executeAction(const StressDecision& decision);
  
  // Session management
  void startSession();
  void endSession(const String& reason = "KLAAR!");
  bool isSessionActive() const { return sessionActive; }
  uint32_t getSessionDuration() const;
  
  // Stress level management
  StressLevel getCurrentStressLevel() const { return currentStressLevel; }
  String getStressLevelName(StressLevel level) const;
  uint32_t getTimeInCurrentLevel() const;
  
  // ML integration
  void enableML(bool enable) { mlEnabled = enable; }
  bool isMLEnabled() const { return mlEnabled; }
  MLTrainingData generateTrainingData(const StressDecision& decision, 
                                     StressAction actionTaken,
                                     const String& userResponse = "");
  
  // ML Autonomy management
  void setMLAutonomyLevel(float level);      // 0.0-1.0: stel autonomie niveau in
  float getMLAutonomyLevel() const { return currentAutonomyLevel; }
  bool isMLAutonomyActive() const { return mlAutonomyActive; }
  void provideFeedback(bool wasGoodDecision);  // Gebruiker feedback voor learning
  void resetMLAutonomy();                      // Reset autonomie (bijvoorbeeld na slechte ervaring)
  
  // Configuration
  void updateConfig(const BodyConfig& config);
  
  // Stress change detection
  StressChangeType getLastStressChange() const;
  String getStressChangeDescription(StressChangeType change) const;
  
  // Debug/monitoring
  void printStatus() const;
  String getStatusString() const;
};

// ===== Global Instance =====
extern AdvancedStressManager stressManager;

// ===== Helper Functions =====

// Convert stress level to speed (1-7)
inline uint8_t stressLevelToSpeed(StressLevel level) {
  switch(level) {
    case STRESS_0_NORMAAL: return 1;
    case STRESS_1_GEEN: return 2;
    case STRESS_2_BEETJE: return 3;
    case STRESS_3_IETS_MEER: return 4;
    case STRESS_4_GEMIDDELD: return 4; // Base, maar reactief
    case STRESS_5_MEER: return 5;      // Base, maar reactief  
    case STRESS_6_VEEL: return 6;      // Base, maar reactief
    case STRESS_7_MAX: return 7;       // Maximum
    default: return 1;
  }
}

// Determine if Vibe should be active for stress level
inline bool shouldVibeBeActive(StressLevel level) {
  return level >= STRESS_2_BEETJE; // Vibe vanaf stress 2
}

// Determine if Suction should be active for stress level  
inline bool shouldSuctionBeActive(StressLevel level) {
  return level >= STRESS_2_BEETJE; // Zuigen vanaf stress 2
}

// CSV formatting for ML training data
String formatMLTrainingDataCSV(const MLTrainingData& data);

#endif // ADVANCED_STRESS_MANAGER_H