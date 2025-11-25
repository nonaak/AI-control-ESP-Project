/*
  ML ANNOTATIE SYSTEEM - Implementatie
  
  ═══════════════════════════════════════════════════════════════════════════
  Slaat handmatige stress level annotaties op voor ML training
  ═══════════════════════════════════════════════════════════════════════════
*/

#include "ml_annotation.h"

// Global instance
MLAnnotationManager mlAnnotations;

// ═══════════════════════════════════════════════════════════════════════════
//                         CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

MLAnnotationManager::MLAnnotationManager() {
  fileOpen = false;
  currentFilename = "";
  annotationFilename = "";
  annotations = nullptr;
  annotationCount = 0;
  annotationCapacity = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         FILE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

bool MLAnnotationManager::openFile(const String& csvFilename) {
  // Sluit eventueel open bestand
  if (fileOpen) {
    closeFile();
  }
  
  currentFilename = csvFilename;
  
  // Maak annotatie filename (.ann)
  annotationFilename = csvFilename;
  annotationFilename.replace(".csv", ML_ANNOTATION_EXT);
  annotationFilename.replace(".anl", ML_ANNOTATION_EXT);
  
  // Voeg pad toe als nodig
  if (!annotationFilename.startsWith("/")) {
    annotationFilename = "/recordings/" + annotationFilename;
  }
  
  Serial.printf("[ML ANN] Opening annotation file: %s\n", annotationFilename.c_str());
  
  // Alloceer buffer
  if (!annotations) {
    annotationCapacity = 100;  // Start klein
    annotations = (StressAnnotation*)malloc(sizeof(StressAnnotation) * annotationCapacity);
    if (!annotations) {
      Serial.println("[ML ANN] ERROR: Could not allocate memory!");
      return false;
    }
  }
  annotationCount = 0;
  
  // Laad bestaande annotaties indien aanwezig
  if (SD_MMC.exists(annotationFilename.c_str())) {
    loadAnnotations();
    Serial.printf("[ML ANN] Loaded %d existing annotations\n", annotationCount);
  } else {
    Serial.println("[ML ANN] No existing annotations, starting fresh");
  }
  
  fileOpen = true;
  return true;
}

void MLAnnotationManager::closeFile() {
  if (!fileOpen) return;
  
  // Sla annotaties op
  if (annotationCount > 0) {
    saveAnnotations();
    Serial.printf("[ML ANN] Saved %d annotations to %s\n", 
                  annotationCount, annotationFilename.c_str());
  }
  
  fileOpen = false;
  currentFilename = "";
  annotationFilename = "";
}

bool MLAnnotationManager::loadAnnotations() {
  File f = SD_MMC.open(annotationFilename.c_str(), FILE_READ);
  if (!f) return false;
  
  // Lees header
  String header = f.readStringUntil('\n');
  
  annotationCount = 0;
  
  while (f.available() && annotationCount < annotationCapacity) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5) continue;
    
    StressAnnotation& ann = annotations[annotationCount];
    memset(&ann, 0, sizeof(ann));
    
    // Parse CSV: timestamp,line,hr,temp,gsr,ai_level,user_level,is_edge,is_orgasm,note,time,was_correction
    int fieldNum = 0;
    int lastComma = -1;
    
    for (int i = 0; i <= line.length(); i++) {
      if (i == line.length() || line.charAt(i) == ',') {
        String field = line.substring(lastComma + 1, i);
        
        switch (fieldNum) {
          case 0: ann.timestamp = field.toFloat(); break;
          case 1: ann.lineNumber = field.toInt(); break;
          case 2: ann.heartRate = field.toFloat(); break;
          case 3: ann.temperature = field.toFloat(); break;
          case 4: ann.gsr = field.toFloat(); break;
          case 5: ann.aiPredictedLevel = field.toInt(); break;
          case 6: ann.userAnnotatedLevel = field.toInt(); break;
          case 7: ann.isEdgeMoment = (field.toInt() == 1); break;
          case 8: ann.isOrgasmMoment = (field.toInt() == 1); break;
          case 9: strncpy(ann.note, field.c_str(), 31); break;
          case 10: ann.annotationTime = field.toInt(); break;
          case 11: ann.wasCorrection = (field.toInt() == 1); break;
        }
        
        lastComma = i;
        fieldNum++;
      }
    }
    
    annotationCount++;
  }
  
  f.close();
  return true;
}

bool MLAnnotationManager::saveAnnotations() {
  File f = SD_MMC.open(annotationFilename.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println("[ML ANN] ERROR: Could not save annotations!");
    return false;
  }
  
  // Header
  f.println("Timestamp,Line,HR,Temp,GSR,AI_Level,User_Level,Is_Edge,Is_Orgasm,Note,Ann_Time,Was_Correction");
  
  // Data
  for (int i = 0; i < annotationCount; i++) {
    StressAnnotation& ann = annotations[i];
    f.printf("%.2f,%lu,%.1f,%.2f,%.0f,%d,%d,%d,%d,%s,%lu,%d\n",
             ann.timestamp,
             ann.lineNumber,
             ann.heartRate,
             ann.temperature,
             ann.gsr,
             ann.aiPredictedLevel,
             ann.userAnnotatedLevel,
             ann.isEdgeMoment ? 1 : 0,
             ann.isOrgasmMoment ? 1 : 0,
             ann.note,
             ann.annotationTime,
             ann.wasCorrection ? 1 : 0);
  }
  
  f.close();
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ANNOTATIES TOEVOEGEN
// ═══════════════════════════════════════════════════════════════════════════

bool MLAnnotationManager::addAnnotation(float timestamp, int userLevel,
                                         float hr, float temp, float gsr,
                                         int aiLevel) {
  if (!fileOpen) {
    Serial.println("[ML ANN] ERROR: No file open!");
    return false;
  }
  
  // Check of er al een annotatie is op deze timestamp
  int existingIdx = findAnnotationIndex(timestamp);
  if (existingIdx >= 0) {
    // Update bestaande annotatie
    Serial.printf("[ML ANN] Updating existing annotation at %.2fs\n", timestamp);
    annotations[existingIdx].userAnnotatedLevel = userLevel;
    annotations[existingIdx].wasCorrection = true;
    annotations[existingIdx].annotationTime = millis() / 1000;  // Simpele timestamp
    return true;
  }
  
  // Check capacity
  if (annotationCount >= annotationCapacity) {
    // Probeer te groeien
    int newCapacity = annotationCapacity * 2;
    if (newCapacity > ML_MAX_ANNOTATIONS) newCapacity = ML_MAX_ANNOTATIONS;
    
    if (annotationCount >= newCapacity) {
      Serial.println("[ML ANN] ERROR: Max annotations reached!");
      return false;
    }
    
    StressAnnotation* newBuffer = (StressAnnotation*)realloc(annotations,
                                    sizeof(StressAnnotation) * newCapacity);
    if (newBuffer) {
      annotations = newBuffer;
      annotationCapacity = newCapacity;
    } else {
      Serial.println("[ML ANN] ERROR: Could not grow buffer!");
      return false;
    }
  }
  
  // Voeg nieuwe annotatie toe
  StressAnnotation& ann = annotations[annotationCount];
  memset(&ann, 0, sizeof(ann));
  
  ann.timestamp = timestamp;
  ann.lineNumber = (uint32_t)(timestamp * 10);  // Schatting: 10 samples/sec
  ann.heartRate = hr;
  ann.temperature = temp;
  ann.gsr = gsr;
  ann.aiPredictedLevel = aiLevel;
  ann.userAnnotatedLevel = userLevel;
  ann.isEdgeMoment = false;
  ann.isOrgasmMoment = false;
  ann.annotationTime = millis() / 1000;
  ann.wasCorrection = false;
  
  annotationCount++;
  
  Serial.printf("[ML ANN] Added annotation #%d at %.2fs: User=%d (AI=%d)\n",
                annotationCount, timestamp, userLevel, aiLevel);
  
  // Auto-save elke 10 annotaties
  if (annotationCount % 10 == 0) {
    saveAnnotations();
  }
  
  return true;
}

bool MLAnnotationManager::addAnnotationAtLine(uint32_t lineNumber, int userLevel,
                                               float hr, float temp, float gsr,
                                               int aiLevel) {
  // Converteer line naar approximate timestamp
  float timestamp = lineNumber / 10.0f;  // Aanname: 10 samples/sec
  
  // Voeg toe met berekende timestamp
  bool result = addAnnotation(timestamp, userLevel, hr, temp, gsr, aiLevel);
  
  // Update line number in laatste annotatie
  if (result && annotationCount > 0) {
    annotations[annotationCount - 1].lineNumber = lineNumber;
  }
  
  return result;
}

bool MLAnnotationManager::markAsEdge(float timestamp) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    annotations[idx].isEdgeMoment = true;
    Serial.printf("[ML ANN] Marked %.2fs as EDGE moment\n", timestamp);
    return true;
  }
  
  // Maak nieuwe annotatie alleen met edge marker
  if (addAnnotation(timestamp, -1, 0, 0, 0, -1)) {
    annotations[annotationCount - 1].isEdgeMoment = true;
    return true;
  }
  
  return false;
}

bool MLAnnotationManager::markAsOrgasm(float timestamp) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    annotations[idx].isOrgasmMoment = true;
    Serial.printf("[ML ANN] Marked %.2fs as ORGASM moment\n", timestamp);
    return true;
  }
  
  // Maak nieuwe annotatie alleen met orgasm marker
  if (addAnnotation(timestamp, 7, 0, 0, 0, -1)) {  // Level 7 = edge zone/orgasm
    annotations[annotationCount - 1].isOrgasmMoment = true;
    return true;
  }
  
  return false;
}

bool MLAnnotationManager::addNote(float timestamp, const char* note) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    strncpy(annotations[idx].note, note, 31);
    annotations[idx].note[31] = '\0';
    return true;
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         ANNOTATIES OPHALEN
// ═══════════════════════════════════════════════════════════════════════════

int MLAnnotationManager::findAnnotationIndex(float timestamp, float tolerance) {
  for (int i = 0; i < annotationCount; i++) {
    if (abs(annotations[i].timestamp - timestamp) <= tolerance) {
      return i;
    }
  }
  return -1;
}

bool MLAnnotationManager::getAnnotationAtTime(float timestamp, StressAnnotation& out) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    out = annotations[idx];
    return true;
  }
  return false;
}

bool MLAnnotationManager::getAnnotationAtLine(uint32_t lineNumber, StressAnnotation& out) {
  for (int i = 0; i < annotationCount; i++) {
    if (annotations[i].lineNumber == lineNumber) {
      out = annotations[i];
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         CORRECTIES
// ═══════════════════════════════════════════════════════════════════════════

bool MLAnnotationManager::updateAnnotation(float timestamp, int newUserLevel) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    annotations[idx].userAnnotatedLevel = newUserLevel;
    annotations[idx].wasCorrection = true;
    annotations[idx].annotationTime = millis() / 1000;
    
    Serial.printf("[ML ANN] Updated annotation at %.2fs: new level = %d\n", 
                  timestamp, newUserLevel);
    return true;
  }
  return false;
}

bool MLAnnotationManager::deleteAnnotation(float timestamp) {
  int idx = findAnnotationIndex(timestamp);
  if (idx >= 0) {
    // Shift all annotations after this one
    for (int i = idx; i < annotationCount - 1; i++) {
      annotations[i] = annotations[i + 1];
    }
    annotationCount--;
    
    Serial.printf("[ML ANN] Deleted annotation at %.2fs\n", timestamp);
    return true;
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         EXPORT VOOR ML TRAINING
// ═══════════════════════════════════════════════════════════════════════════

bool MLAnnotationManager::exportForTraining(const String& outputFilename) {
  if (annotationCount == 0) {
    Serial.println("[ML ANN] No annotations to export!");
    return false;
  }
  
  // Maak output path
  String outPath = ML_ANNOTATION_DIR;
  outPath += "/" + outputFilename;
  
  File f = SD_MMC.open(outPath.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println("[ML ANN] ERROR: Could not create export file!");
    return false;
  }
  
  // Header in ML training formaat
  f.println("Timestamp,SessionTime,HR,HR_Avg,HR_Trend,Temp,Temp_Delta,GSR,GSR_Avg,GSR_Trend,EdgeCount,TimeSinceEdge,CurrentLevel,AI_Level,User_Level,Correction");
  
  int edgeCount = 0;
  float lastEdgeTime = 0;
  
  for (int i = 0; i < annotationCount; i++) {
    StressAnnotation& ann = annotations[i];
    
    // Skip annotaties zonder user level
    if (ann.userAnnotatedLevel < 0) continue;
    
    // Track edges
    if (ann.isEdgeMoment) {
      edgeCount++;
      lastEdgeTime = ann.timestamp;
    }
    
    float timeSinceEdge = (lastEdgeTime > 0) ? (ann.timestamp - lastEdgeTime) : -1;
    int correction = ann.userAnnotatedLevel - ann.aiPredictedLevel;
    
    // Schrijf in ML training formaat
    // Note: sommige velden zijn 0 omdat we geen rolling averages hebben in playback
    f.printf("%.0f,%.0f,%.1f,%.1f,0,%.2f,%.2f,%.0f,%.0f,0,%d,%.1f,%d,%d,%d,%d\n",
             ann.timestamp * 1000,        // Timestamp in ms
             ann.timestamp,               // Session time in sec
             ann.heartRate,               // HR
             ann.heartRate,               // HR Avg (zelfde, geen rolling avg)
             ann.temperature,             // Temp
             ann.temperature - 36.5f,     // Temp delta
             ann.gsr,                     // GSR
             ann.gsr,                     // GSR Avg (zelfde)
             edgeCount,                   // Edge count
             timeSinceEdge,               // Time since edge
             ann.aiPredictedLevel,        // Current/AI level
             ann.aiPredictedLevel,        // AI chosen
             ann.userAnnotatedLevel,      // User chosen
             correction);                 // Correction
  }
  
  f.close();
  
  Serial.printf("[ML ANN] Exported %d annotations to %s\n", annotationCount, outPath.c_str());
  Serial.println("[ML ANN] Dit bestand kan worden gebruikt voor ML training!");
  
  return true;
}

bool MLAnnotationManager::mergeWithANL(const String& anlFilename) {
  if (annotationCount == 0) {
    Serial.println("[ML ANN] No annotations to merge!");
    return false;
  }
  
  // Open ANL bestand
  String anlPath = "/recordings/" + anlFilename;
  if (!SD_MMC.exists(anlPath.c_str())) {
    Serial.println("[ML ANN] ANL file not found!");
    return false;
  }
  
  // Lees hele ANL bestand
  File f = SD_MMC.open(anlPath.c_str(), FILE_READ);
  if (!f) return false;
  
  // Lees header
  String header = f.readStringUntil('\n');
  
  // Lees alle regels in geheugen (tijdelijk)
  String* lines = new String[10000];  // Max 10000 regels
  int lineCount = 0;
  
  while (f.available() && lineCount < 10000) {
    lines[lineCount] = f.readStringUntil('\n');
    lines[lineCount].trim();
    lineCount++;
  }
  f.close();
  
  Serial.printf("[ML ANN] Read %d lines from ANL\n", lineCount);
  
  // Update regels met annotaties
  int updatedCount = 0;
  for (int a = 0; a < annotationCount; a++) {
    StressAnnotation& ann = annotations[a];
    if (ann.userAnnotatedLevel < 0) continue;
    
    // Vind corresponderende regel (op basis van timestamp)
    uint32_t targetLine = (uint32_t)(ann.timestamp * 10);  // ~10 samples/sec
    
    if (targetLine < lineCount) {
      // Vervang laatste kolom (StressLevel) met user annotatie
      String& line = lines[targetLine];
      int lastComma = line.lastIndexOf(',');
      if (lastComma > 0) {
        line = line.substring(0, lastComma + 1) + String(ann.userAnnotatedLevel);
        updatedCount++;
      }
    }
  }
  
  Serial.printf("[ML ANN] Updated %d lines with user annotations\n", updatedCount);
  
  // Schrijf terug naar ANL (maak backup eerst)
  String backupPath = anlPath + ".bak";
  SD_MMC.rename(anlPath.c_str(), backupPath.c_str());
  
  f = SD_MMC.open(anlPath.c_str(), FILE_WRITE);
  if (!f) {
    // Herstel backup
    SD_MMC.rename(backupPath.c_str(), anlPath.c_str());
    delete[] lines;
    return false;
  }
  
  f.println(header);
  for (int i = 0; i < lineCount; i++) {
    f.println(lines[i]);
  }
  f.close();
  
  delete[] lines;
  
  // Verwijder backup
  SD_MMC.remove(backupPath.c_str());
  
  Serial.printf("[ML ANN] Merged annotations into %s\n", anlPath.c_str());
  
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         STATISTICS
// ═══════════════════════════════════════════════════════════════════════════

int MLAnnotationManager::getTotalAnnotations() {
  return annotationCount;
}

int MLAnnotationManager::getEdgeCount() {
  int count = 0;
  for (int i = 0; i < annotationCount; i++) {
    if (annotations[i].isEdgeMoment) count++;
  }
  return count;
}

float MLAnnotationManager::getAverageCorrection() {
  if (annotationCount == 0) return 0;
  
  float totalCorrection = 0;
  int validCount = 0;
  
  for (int i = 0; i < annotationCount; i++) {
    if (annotations[i].userAnnotatedLevel >= 0 && annotations[i].aiPredictedLevel >= 0) {
      totalCorrection += abs(annotations[i].userAnnotatedLevel - annotations[i].aiPredictedLevel);
      validCount++;
    }
  }
  
  return validCount > 0 ? totalCorrection / validCount : 0;
}

void MLAnnotationManager::printAnnotations() {
  Serial.println("[ML ANN] ═══════════════════════════════════════════════");
  Serial.printf("[ML ANN] Annotations for: %s\n", currentFilename.c_str());
  Serial.println("[ML ANN] ─────────────────────────────────────────────────");
  
  for (int i = 0; i < annotationCount; i++) {
    StressAnnotation& ann = annotations[i];
    Serial.printf("[ML ANN] %3d | t=%.1fs | AI=%d → User=%d", 
                  i+1, ann.timestamp, ann.aiPredictedLevel, ann.userAnnotatedLevel);
    if (ann.isEdgeMoment) Serial.print(" [EDGE]");
    if (ann.isOrgasmMoment) Serial.print(" [ORGASM]");
    if (strlen(ann.note) > 0) Serial.printf(" \"%s\"", ann.note);
    Serial.println();
  }
  
  Serial.println("[ML ANN] ═══════════════════════════════════════════════");
}

void MLAnnotationManager::printStats() {
  Serial.println("[ML ANN] ═══════════════════════════════════════════════");
  Serial.println("[ML ANN] ANNOTATION STATISTICS");
  Serial.println("[ML ANN] ─────────────────────────────────────────────────");
  Serial.printf("[ML ANN] Total annotations: %d\n", annotationCount);
  Serial.printf("[ML ANN] Edge moments: %d\n", getEdgeCount());
  Serial.printf("[ML ANN] Average correction: %.2f levels\n", getAverageCorrection());
  Serial.printf("[ML ANN] File: %s\n", annotationFilename.c_str());
  Serial.println("[ML ANN] ═══════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
//                    GLOBAL CONVENIENCE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

bool ml_openAnnotationFile(const String& csvFilename) {
  return mlAnnotations.openFile(csvFilename);
}

void ml_closeAnnotationFile() {
  mlAnnotations.closeFile();
}

bool ml_addPlaybackAnnotation(float playbackTimestamp, int userStressLevel,
                               float hr, float temp, float gsr, int aiLevel) {
  return mlAnnotations.addAnnotation(playbackTimestamp, userStressLevel, 
                                      hr, temp, gsr, aiLevel);
}

bool ml_hasAnnotationAt(float timestamp) {
  StressAnnotation dummy;
  return mlAnnotations.getAnnotationAtTime(timestamp, dummy);
}

int ml_getAnnotationAt(float timestamp) {
  StressAnnotation ann;
  if (mlAnnotations.getAnnotationAtTime(timestamp, ann)) {
    return ann.userAnnotatedLevel;
  }
  return -1;  // Geen annotatie
}

bool ml_markEdgeAt(float timestamp) {
  return mlAnnotations.markAsEdge(timestamp);
}

bool ml_markOrgasmAt(float timestamp) {
  return mlAnnotations.markAsOrgasm(timestamp);
}

bool ml_exportAnnotationsForTraining() {
  // Export naar standaard training bestand
  return mlAnnotations.exportForTraining("playback_annotations.csv");
}

bool ml_mergeAnnotationsWithANL() {
  String anlFilename = mlAnnotations.getCurrentFilename();
  anlFilename.replace(".csv", ".anl");
  anlFilename.replace("/recordings/", "");
  return mlAnnotations.mergeWithANL(anlFilename);
}
