/*
  ML Data Parser - .aly en .csv bestand parsers
  
  .aly bestanden: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe,StressLevel
  .csv bestanden: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe
  
  StressLevel = 1-7 (stress levels)
  Trust = bewegingssnelheid (thrust speed)
  SleevePos = sleeve positie (0-100%)
*/

#ifndef ML_DATA_PARSER_H
#define ML_DATA_PARSER_H

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include "ml_decision_tree.h"

// ===== Data Statistics =====

struct DatasetStats {
  int totalSamples;
  int stressLevelCounts[7];   // Count per stress level (1-7)
  float avgHR, avgTemp, avgGSR, avgAdem;
  String filename;
  bool isLabeled;             // .aly = true, .csv = false
  
  DatasetStats() : totalSamples(0), avgHR(0), avgTemp(0), avgGSR(0), avgAdem(0), isLabeled(false) {
    memset(stressLevelCounts, 0, sizeof(stressLevelCounts));
  }
};

// ===== Parser Functions =====

// Parse .aly bestand (met labels)
bool parseAlyFile(const char* filename, std::vector<TrainingSample>& samples, DatasetStats& stats);

// Parse .csv bestand (zonder labels)
bool parseCsvFile(const char* filename, std::vector<TrainingSample>& samples, DatasetStats& stats);

// Save .aly bestand (voor AI annotatie workflow)
bool saveAlyFile(const char* filename, const std::vector<TrainingSample>& samples);

// Quick file info zonder alle data te laden
bool getFileStats(const char* filename, DatasetStats& stats);

// Helper: parse single CSV line naar features
bool parseCsvLine(const String& line, float features[9], int& label);

// Helper: validate bestandsformaat
bool validateAlyFile(const char* filename);
bool validateCsvFile(const char* filename);

#endif // ML_DATA_PARSER_H
