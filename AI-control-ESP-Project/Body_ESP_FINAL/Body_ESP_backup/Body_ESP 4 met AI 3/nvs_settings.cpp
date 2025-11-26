/*
  NVS SETTINGS - Implementatie
  
  Centrale opslag voor alle instellingen in ESP32 NVS
*/

#include "nvs_settings.h"

// ═══════════════════════════════════════════════════════════════════════════
//                         CONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════

const char* ML_SLOT_NAMES[ML_MODEL_SLOTS] = {
  "LANGZAAM",    // Slot A
  "SNEL",        // Slot B
  "EXPERIMENT"   // Slot C
};

// Namespace namen
static const char* NS_AI = "ai_settings";
static const char* NS_SENSOR = "sensor_cal";
static const char* NS_DISPLAY = "display_cfg";
static const char* NS_TOUCH = "touch_cfg";
static const char* NS_ML_ACTIVE = "ml_active";
static const char* NS_STATS = "stats";

// Model namespaces
static const char* NS_ML_MODELS[ML_MODEL_SLOTS] = {
  "ml_model_a",
  "ml_model_b", 
  "ml_model_c"
};

// Magic number voor model validatie
static const uint32_t MODEL_MAGIC = 0x4D4C4D44;  // "MLMD"

// Preferences object
static Preferences prefs;

// ═══════════════════════════════════════════════════════════════════════════
//                         INITIALISATIE
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_begin() {
  Serial.println("[NVS] ═══════════════════════════════════════════════");
  Serial.println("[NVS] NVS Settings System Startup");
  Serial.println("[NVS] ═══════════════════════════════════════════════");
  
  // Check NVS beschikbaarheid
  prefs.begin("_nvs_test", false);
  prefs.putUInt("test", 12345);
  uint32_t test = prefs.getUInt("test", 0);
  prefs.end();
  
  if (test == 12345) {
    Serial.println("[NVS] ✅ NVS beschikbaar en werkend");
  } else {
    Serial.println("[NVS] ❌ NVS PROBLEEM!");
  }
  
  // Print model status
  Serial.println("[NVS] ML Model Status:");
  for (int i = 0; i < ML_MODEL_SLOTS; i++) {
    MLModelInfo info;
    if (nvsSettings_getModelInfo(i, &info) && info.exists) {
      Serial.printf("[NVS]   Slot %c (%s): %.1f%% accuracy, %d sessies\n",
                    'A' + i, ML_SLOT_NAMES[i], info.accuracy * 100, info.sessions);
    } else {
      Serial.printf("[NVS]   Slot %c (%s): leeg\n", 'A' + i, ML_SLOT_NAMES[i]);
    }
  }
  
  uint8_t active = nvsSettings_getActiveModel();
  Serial.printf("[NVS] Actief model: Slot %c (%s)\n", 'A' + active, ML_SLOT_NAMES[active]);
  
  Serial.println("[NVS] ═══════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         AI SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadAI(AISettingsNVS* settings) {
  prefs.begin(NS_AI, true);  // read-only
  
  settings->autonomyLevel = prefs.getFloat("autonomy", 90.0f);
  settings->hrLow = prefs.getFloat("hr_low", 60.0f);
  settings->hrHigh = prefs.getFloat("hr_high", 140.0f);
  settings->tempMax = prefs.getFloat("temp_max", 37.5f);
  settings->gsrMax = prefs.getFloat("gsr_max", 500.0f);
  settings->responseRate = prefs.getFloat("response", 0.5f);
  settings->aiEnabled = prefs.getBool("ai_enabled", false);
  
  prefs.end();
  
  Serial.println("[NVS] AI Settings geladen");
}

void nvsSettings_saveAI(const AISettingsNVS* settings) {
  prefs.begin(NS_AI, false);  // read-write
  
  prefs.putFloat("autonomy", settings->autonomyLevel);
  prefs.putFloat("hr_low", settings->hrLow);
  prefs.putFloat("hr_high", settings->hrHigh);
  prefs.putFloat("temp_max", settings->tempMax);
  prefs.putFloat("gsr_max", settings->gsrMax);
  prefs.putFloat("response", settings->responseRate);
  prefs.putBool("ai_enabled", settings->aiEnabled);
  
  prefs.end();
  
  Serial.println("[NVS] ✅ AI Settings opgeslagen");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         SENSOR KALIBRATIE
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadSensorCal(SensorCalibrationNVS* cal) {
  prefs.begin(NS_SENSOR, true);
  
  cal->beatThreshold = prefs.getFloat("beat_thr", 50000.0f);
  cal->tempOffset = prefs.getFloat("temp_off", 0.0f);
  cal->tempSmoothing = prefs.getFloat("temp_smooth", 0.2f);
  cal->gsrBaseline = prefs.getFloat("gsr_base", 500.0f);
  cal->gsrSensitivity = prefs.getFloat("gsr_sens", 1.0f);
  cal->gsrSmoothing = prefs.getFloat("gsr_smooth", 0.3f);
  
  prefs.end();
  
  Serial.println("[NVS] Sensor Kalibratie geladen");
}

void nvsSettings_saveSensorCal(const SensorCalibrationNVS* cal) {
  prefs.begin(NS_SENSOR, false);
  
  prefs.putFloat("beat_thr", cal->beatThreshold);
  prefs.putFloat("temp_off", cal->tempOffset);
  prefs.putFloat("temp_smooth", cal->tempSmoothing);
  prefs.putFloat("gsr_base", cal->gsrBaseline);
  prefs.putFloat("gsr_sens", cal->gsrSensitivity);
  prefs.putFloat("gsr_smooth", cal->gsrSmoothing);
  
  prefs.end();
  
  Serial.println("[NVS] ✅ Sensor Kalibratie opgeslagen");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DISPLAY SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadDisplay(DisplaySettingsNVS* settings) {
  prefs.begin(NS_DISPLAY, true);
  
  settings->rotation = prefs.getUChar("rotation", 1);
  settings->brightness = prefs.getUChar("brightness", 255);
  
  prefs.end();
}

void nvsSettings_saveDisplay(const DisplaySettingsNVS* settings) {
  prefs.begin(NS_DISPLAY, false);
  
  prefs.putUChar("rotation", settings->rotation);
  prefs.putUChar("brightness", settings->brightness);
  
  prefs.end();
  
  Serial.println("[NVS] ✅ Display Settings opgeslagen");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         TOUCH SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadTouch(TouchSettingsNVS* settings) {
  prefs.begin(NS_TOUCH, true);
  
  settings->menuEnabled = prefs.getBool("touch_menu", true);
  settings->paramsEnabled = prefs.getBool("touch_params", true);
  settings->emergencyEnabled = prefs.getBool("touch_emerg", true);
  
  prefs.end();
}

void nvsSettings_saveTouch(const TouchSettingsNVS* settings) {
  prefs.begin(NS_TOUCH, false);
  
  prefs.putBool("touch_menu", settings->menuEnabled);
  prefs.putBool("touch_params", settings->paramsEnabled);
  prefs.putBool("touch_emerg", settings->emergencyEnabled);
  
  prefs.end();
  
  Serial.println("[NVS] ✅ Touch Settings opgeslagen");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ML MODELLEN - ACTIEF MODEL
// ═══════════════════════════════════════════════════════════════════════════

uint8_t nvsSettings_getActiveModel() {
  prefs.begin(NS_ML_ACTIVE, true);
  uint8_t slot = prefs.getUChar("active", 0);  // Default: Slot A
  prefs.end();
  
  if (slot >= ML_MODEL_SLOTS) slot = 0;
  return slot;
}

void nvsSettings_setActiveModel(uint8_t slot) {
  if (slot >= ML_MODEL_SLOTS) slot = 0;
  
  prefs.begin(NS_ML_ACTIVE, false);
  prefs.putUChar("active", slot);
  prefs.end();
  
  Serial.printf("[NVS] ✅ Actief model: Slot %c (%s)\n", 'A' + slot, ML_SLOT_NAMES[slot]);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ML MODELLEN - INFO
// ═══════════════════════════════════════════════════════════════════════════

bool nvsSettings_getModelInfo(uint8_t slot, MLModelInfo* info) {
  if (slot >= ML_MODEL_SLOTS || !info) return false;
  
  prefs.begin(NS_ML_MODELS[slot], true);
  
  uint32_t magic = prefs.getUInt("magic", 0);
  info->exists = (magic == MODEL_MAGIC);
  
  if (info->exists) {
    info->accuracy = prefs.getFloat("accuracy", 0.0f);
    info->sessions = prefs.getUShort("sessions", 0);
    info->lastTrained = prefs.getUInt("trained", 0);
    info->sampleCount = prefs.getUShort("samples", 0);
  } else {
    info->accuracy = 0.0f;
    info->sessions = 0;
    info->lastTrained = 0;
    info->sampleCount = 0;
  }
  
  prefs.end();
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ML MODELLEN - OPSLAAN/LADEN
// ═══════════════════════════════════════════════════════════════════════════

bool nvsSettings_saveModel(uint8_t slot, const uint8_t* data, uint16_t size, float accuracy) {
  if (slot >= ML_MODEL_SLOTS || !data || size == 0 || size > 1024) {
    Serial.println("[NVS] ❌ Model save: ongeldige parameters");
    return false;
  }
  
  Serial.printf("[NVS] 💾 Saving model to Slot %c...\n", 'A' + slot);
  
  prefs.begin(NS_ML_MODELS[slot], false);
  
  // Metadata
  prefs.putUInt("magic", MODEL_MAGIC);
  prefs.putFloat("accuracy", accuracy);
  prefs.putUShort("size", size);
  prefs.putUInt("trained", millis() / 1000);  // Relative timestamp
  
  // Haal bestaande sessie count op en behoud
  uint16_t sessions = prefs.getUShort("sessions", 0);
  prefs.putUShort("sessions", sessions + 1);  // +1 voor deze training
  
  // Model data
  size_t written = prefs.putBytes("data", data, size);
  
  prefs.end();
  
  if (written == size) {
    Serial.printf("[NVS] ✅ Model opgeslagen in Slot %c (%d bytes, %.1f%% accuracy)\n",
                  'A' + slot, size, accuracy * 100);
    return true;
  } else {
    Serial.printf("[NVS] ❌ Model save failed (wrote %d of %d bytes)\n", written, size);
    return false;
  }
}

bool nvsSettings_loadModel(uint8_t slot, uint8_t* data, uint16_t* size) {
  if (slot >= ML_MODEL_SLOTS || !data || !size) return false;
  
  prefs.begin(NS_ML_MODELS[slot], true);
  
  uint32_t magic = prefs.getUInt("magic", 0);
  if (magic != MODEL_MAGIC) {
    prefs.end();
    Serial.printf("[NVS] Slot %c: geen model\n", 'A' + slot);
    return false;
  }
  
  uint16_t storedSize = prefs.getUShort("size", 0);
  if (storedSize == 0 || storedSize > 1024) {
    prefs.end();
    Serial.printf("[NVS] Slot %c: ongeldige grootte\n", 'A' + slot);
    return false;
  }
  
  *size = storedSize;
  size_t read = prefs.getBytes("data", data, storedSize);
  
  float accuracy = prefs.getFloat("accuracy", 0.0f);
  
  prefs.end();
  
  if (read == storedSize) {
    Serial.printf("[NVS] ✅ Model geladen uit Slot %c (%d bytes, %.1f%% accuracy)\n",
                  'A' + slot, storedSize, accuracy * 100);
    return true;
  }
  
  return false;
}

bool nvsSettings_deleteModel(uint8_t slot) {
  if (slot >= ML_MODEL_SLOTS) return false;
  
  prefs.begin(NS_ML_MODELS[slot], false);
  prefs.clear();  // Wis alle data in deze namespace
  prefs.end();
  
  Serial.printf("[NVS] 🗑️ Model Slot %c gewist\n", 'A' + slot);
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ML MODELLEN - STATS UPDATE
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_incrementModelSessions(uint8_t slot) {
  if (slot >= ML_MODEL_SLOTS) return;
  
  prefs.begin(NS_ML_MODELS[slot], false);
  uint16_t sessions = prefs.getUShort("sessions", 0);
  prefs.putUShort("sessions", sessions + 1);
  prefs.end();
}

void nvsSettings_updateModelAccuracy(uint8_t slot, float newAccuracy, uint16_t newSamples) {
  if (slot >= ML_MODEL_SLOTS) return;
  
  prefs.begin(NS_ML_MODELS[slot], false);
  prefs.putFloat("accuracy", newAccuracy);
  prefs.putUShort("samples", newSamples);
  prefs.putUInt("trained", millis() / 1000);
  prefs.end();
  
  Serial.printf("[NVS] Model Slot %c updated: %.1f%% accuracy, %d samples\n",
                'A' + slot, newAccuracy * 100, newSamples);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         SESSIE STATS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadStats(SessionStatsNVS* stats) {
  prefs.begin(NS_STATS, true);
  
  stats->totalSessions = prefs.getUInt("sessions", 0);
  stats->totalEdges = prefs.getUInt("edges", 0);
  stats->totalOrgasms = prefs.getUInt("orgasms", 0);
  stats->longestSession = prefs.getUInt("longest", 0);
  stats->totalTime = prefs.getUInt("total_time", 0);
  
  prefs.end();
}

void nvsSettings_saveStats(const SessionStatsNVS* stats) {
  prefs.begin(NS_STATS, false);
  
  prefs.putUInt("sessions", stats->totalSessions);
  prefs.putUInt("edges", stats->totalEdges);
  prefs.putUInt("orgasms", stats->totalOrgasms);
  prefs.putUInt("longest", stats->longestSession);
  prefs.putUInt("total_time", stats->totalTime);
  
  prefs.end();
}

void nvsSettings_incrementSession(uint32_t durationSeconds, uint8_t edges, bool orgasm) {
  SessionStatsNVS stats;
  nvsSettings_loadStats(&stats);
  
  stats.totalSessions++;
  stats.totalEdges += edges;
  if (orgasm) stats.totalOrgasms++;
  stats.totalTime += durationSeconds;
  if (durationSeconds > stats.longestSession) {
    stats.longestSession = durationSeconds;
  }
  
  nvsSettings_saveStats(&stats);
  
  Serial.printf("[NVS] Sessie stats: #%d, %d edges, %s, %ds\n",
                stats.totalSessions, edges, orgasm ? "orgasme" : "geen orgasme", durationSeconds);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         BACKUP / RESTORE
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_saveAll() {
  Serial.println("[NVS] ═══════════════════════════════════════════════");
  Serial.println("[NVS] Saving ALL settings...");
  Serial.println("[NVS] ═══════════════════════════════════════════════");
  
  // Alle namespaces worden automatisch opgeslagen bij putXxx() calls
  // Deze functie is meer voor expliciete "save now" actie
  
  Serial.println("[NVS] ✅ All settings saved!");
}

void nvsSettings_factoryReset() {
  Serial.println("[NVS] ⚠️ ═══════════════════════════════════════════════");
  Serial.println("[NVS] ⚠️ FACTORY RESET - WISSEN VAN ALLE DATA!");
  Serial.println("[NVS] ⚠️ ═══════════════════════════════════════════════");
  
  // Wis alle namespaces
  const char* namespaces[] = {
    NS_AI, NS_SENSOR, NS_DISPLAY, NS_TOUCH, NS_ML_ACTIVE, NS_STATS,
    NS_ML_MODELS[0], NS_ML_MODELS[1], NS_ML_MODELS[2]
  };
  
  for (int i = 0; i < sizeof(namespaces) / sizeof(namespaces[0]); i++) {
    prefs.begin(namespaces[i], false);
    prefs.clear();
    prefs.end();
    Serial.printf("[NVS] Cleared: %s\n", namespaces[i]);
  }
  
  Serial.println("[NVS] ✅ Factory reset complete!");
  Serial.println("[NVS] Herstart ESP32 voor defaults...");
}

void nvsSettings_printAll() {
  Serial.println("\n[NVS] ═══════════════════════════════════════════════");
  Serial.println("[NVS] ALLE INSTELLINGEN:");
  Serial.println("[NVS] ═══════════════════════════════════════════════");
  
  // AI Settings
  AISettingsNVS ai;
  nvsSettings_loadAI(&ai);
  Serial.println("[NVS] AI SETTINGS:");
  Serial.printf("  Autonomy: %.0f%%\n", ai.autonomyLevel);
  Serial.printf("  HR: %.0f - %.0f BPM\n", ai.hrLow, ai.hrHigh);
  Serial.printf("  Temp Max: %.1f°C\n", ai.tempMax);
  Serial.printf("  GSR Max: %.0f\n", ai.gsrMax);
  Serial.printf("  Response: %.2f\n", ai.responseRate);
  Serial.printf("  Enabled: %s\n", ai.aiEnabled ? "JA" : "NEE");
  
  // Sensor Calibration
  SensorCalibrationNVS sensor;
  nvsSettings_loadSensorCal(&sensor);
  Serial.println("[NVS] SENSOR KALIBRATIE:");
  Serial.printf("  Beat Threshold: %.0f\n", sensor.beatThreshold);
  Serial.printf("  Temp Offset: %.2f°C\n", sensor.tempOffset);
  Serial.printf("  Temp Smooth: %.2f\n", sensor.tempSmoothing);
  Serial.printf("  GSR Baseline: %.0f\n", sensor.gsrBaseline);
  Serial.printf("  GSR Sensitivity: %.2f\n", sensor.gsrSensitivity);
  Serial.printf("  GSR Smooth: %.2f\n", sensor.gsrSmoothing);
  
  // Display
  DisplaySettingsNVS display;
  nvsSettings_loadDisplay(&display);
  Serial.println("[NVS] DISPLAY:");
  Serial.printf("  Rotation: %d\n", display.rotation);
  Serial.printf("  Brightness: %d\n", display.brightness);
  
  // Touch
  TouchSettingsNVS touch;
  nvsSettings_loadTouch(&touch);
  Serial.println("[NVS] TOUCH:");
  Serial.printf("  Menu: %s\n", touch.menuEnabled ? "AAN" : "UIT");
  Serial.printf("  Params: %s\n", touch.paramsEnabled ? "AAN" : "UIT");
  Serial.printf("  Emergency: %s\n", touch.emergencyEnabled ? "AAN" : "UIT");
  
  // ML Models
  Serial.println("[NVS] ML MODELLEN:");
  uint8_t active = nvsSettings_getActiveModel();
  for (int i = 0; i < ML_MODEL_SLOTS; i++) {
    MLModelInfo info;
    nvsSettings_getModelInfo(i, &info);
    Serial.printf("  Slot %c (%s)%s: ", 'A' + i, ML_SLOT_NAMES[i], 
                  (i == active) ? " [ACTIEF]" : "");
    if (info.exists) {
      Serial.printf("%.1f%% accuracy, %d sessies, %d samples\n",
                    info.accuracy * 100, info.sessions, info.sampleCount);
    } else {
      Serial.println("leeg");
    }
  }
  
  // Stats
  SessionStatsNVS stats;
  nvsSettings_loadStats(&stats);
  Serial.println("[NVS] SESSIE STATS:");
  Serial.printf("  Totaal sessies: %d\n", stats.totalSessions);
  Serial.printf("  Totaal edges: %d\n", stats.totalEdges);
  Serial.printf("  Totaal orgasmes: %d\n", stats.totalOrgasms);
  Serial.printf("  Langste sessie: %d sec\n", stats.longestSession);
  Serial.printf("  Totale tijd: %d sec (%.1f uur)\n", stats.totalTime, stats.totalTime / 3600.0f);
  
  Serial.println("[NVS] ═══════════════════════════════════════════════\n");
}
