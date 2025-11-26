/*
  ML ANNOTATIE SYSTEEM
  
  ═══════════════════════════════════════════════════════════════════════════
  HANDMATIGE ANNOTATIES VOOR ML TRAINING
  ═══════════════════════════════════════════════════════════════════════════
  
  Dit systeem slaat handmatige stress level annotaties op die je maakt
  tijdens PLAYBACK via de AI-ACTIE popup. Deze annotaties zijn GOUD
  voor ML training omdat:
  
  1. Je hebt tijd om na te denken over wat je voelde
  2. Je kunt meerdere keren door dezelfde data gaan
  3. Je kunt eerdere annotaties corrigeren
  
  Annotaties worden opgeslagen in .ann bestanden naast de .csv/.anl
*/

#ifndef ML_ANNOTATION_H
#define ML_ANNOTATION_H

#include <Arduino.h>
#include <SD_MMC.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         CONFIGURATIE
// ═══════════════════════════════════════════════════════════════════════════

#define ML_ANNOTATION_DIR       "/ml_training"
#define ML_ANNOTATION_EXT       ".ann"
#define ML_MAX_ANNOTATIONS      500   // Max annotaties per bestand

// ═══════════════════════════════════════════════════════════════════════════
//                    ANNOTATIE STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct StressAnnotation {
  // Timing
  float timestamp;              // Seconden in recording
  uint32_t lineNumber;          // Regel nummer in CSV/ANL
  
  // Sensor data op dit moment (uit playback)
  float heartRate;
  float temperature;
  float gsr;
  
  // AI vs Gebruiker
  int aiPredictedLevel;         // Wat AI had voorspeld (uit .anl of rule-based)
  int userAnnotatedLevel;       // Wat JIJ zegt dat het moet zijn (0-7)
  
  // Extra context
  bool isEdgeMoment;            // Was dit een edge moment?
  bool isOrgasmMoment;          // Was dit het orgasme moment?
  char note[32];                // Optionele notitie
  
  // Metadata
  uint32_t annotationTime;      // Wanneer werd annotatie gemaakt (Unix tijd)
  bool wasCorrection;           // Was dit een correctie van eerdere annotatie?
};

// ═══════════════════════════════════════════════════════════════════════════
//                    ANNOTATIE MANAGER CLASS
// ═══════════════════════════════════════════════════════════════════════════

class MLAnnotationManager {
public:
  MLAnnotationManager();
  
  // ─── File Management ───
  bool openFile(const String& csvFilename);  // Open/maak .ann voor een CSV
  void closeFile();
  bool hasOpenFile() { return fileOpen; }
  String getCurrentFilename() { return currentFilename; }
  
  // ─── Annotaties Toevoegen ───
  bool addAnnotation(float timestamp, int userLevel, 
                     float hr, float temp, float gsr,
                     int aiLevel = -1);
  
  bool addAnnotationAtLine(uint32_t lineNumber, int userLevel,
                           float hr, float temp, float gsr,
                           int aiLevel = -1);
  
  bool markAsEdge(float timestamp);
  bool markAsOrgasm(float timestamp);
  bool addNote(float timestamp, const char* note);
  
  // ─── Annotaties Ophalen ───
  bool getAnnotationAtTime(float timestamp, StressAnnotation& out);
  bool getAnnotationAtLine(uint32_t lineNumber, StressAnnotation& out);
  int getAnnotationCount() { return annotationCount; }
  
  // ─── Correcties ───
  bool updateAnnotation(float timestamp, int newUserLevel);
  bool deleteAnnotation(float timestamp);
  
  // ─── Export voor ML Training ───
  bool exportForTraining(const String& outputFilename);
  bool mergeWithANL(const String& anlFilename);  // Update .anl met annotaties
  
  // ─── Statistics ───
  int getTotalAnnotations();
  int getEdgeCount();
  float getAverageCorrection();  // Gemiddeld verschil AI vs User
  
  // ─── Debug ───
  void printAnnotations();
  void printStats();

private:
  bool fileOpen;
  String currentFilename;
  String annotationFilename;
  
  // In-memory buffer voor snelle toegang
  StressAnnotation* annotations;
  int annotationCount;
  int annotationCapacity;
  
  // ─── Internal ───
  bool loadAnnotations();
  bool saveAnnotations();
  int findAnnotationIndex(float timestamp, float tolerance = 0.5f);
};

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBAL INSTANCE
// ═══════════════════════════════════════════════════════════════════════════

extern MLAnnotationManager mlAnnotations;

// ═══════════════════════════════════════════════════════════════════════════
//                    PLAYBACK INTEGRATIE FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Open annotatie file voor huidige playback
bool ml_openAnnotationFile(const String& csvFilename);

// Sluit en sla op
void ml_closeAnnotationFile();

// Voeg annotatie toe op huidige playback positie
// Roep dit aan vanuit de stress popup OPSLAAN knop!
bool ml_addPlaybackAnnotation(float playbackTimestamp, int userStressLevel,
                               float hr, float temp, float gsr, int aiLevel);

// Check of er al een annotatie is op deze positie
bool ml_hasAnnotationAt(float timestamp);

// Krijg bestaande annotatie (voor weergave in popup)
int ml_getAnnotationAt(float timestamp);  // Returns -1 als geen annotatie

// Markeer huidige positie als edge/orgasme
bool ml_markEdgeAt(float timestamp);
bool ml_markOrgasmAt(float timestamp);

// Export alle annotaties naar ML training formaat
bool ml_exportAnnotationsForTraining();

// Merge annotaties met .anl bestand (update AI levels)
bool ml_mergeAnnotationsWithANL();

#endif // ML_ANNOTATION_H
