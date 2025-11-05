/*
  ML Data Parser Implementation
  
  Parse .aly (labeled) en .csv (unlabeled) bestanden
*/

#include "ml_data_parser.h"

// ===== .ALY File Parsing (with labels) =====

bool parseAlyFile(const char* filename, std::vector<TrainingSample>& samples, DatasetStats& stats) {
  File file = SD.open(filename);
  if (!file) {
    Serial.printf("[PARSER] Fout: kan %s niet openen\n", filename);
    return false;
  }
  
  Serial.printf("[PARSER] Lezen .aly bestand: %s\n", filename);
  
  stats.filename = filename;
  stats.isLabeled = true;
  stats.totalSamples = 0;
  
  float sumHR = 0, sumTemp = 0, sumGSR = 0, sumAdem = 0;
  
  // Skip header lijn
  if (file.available()) {
    String header = file.readStringUntil('\n');
    Serial.printf("[PARSER] Header: %s\n", header.c_str());
  }
  
  // Parse data lijnen
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() < 5) continue; // Skip lege lijnen
    
    // Parse CSV lijn: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe,StressLevel
    TrainingSample sample;
    int label;
    
    if (parseCsvLine(line, sample.features, label)) {
      sample.label = label;
      samples.push_back(sample);
      
      // Update statistieken
      stats.totalSamples++;
      if (label >= 1 && label <= 7) {
        stats.stressLevelCounts[label - 1]++;
      }
      
      // Feature 0=HR, 1=Temp, 2=GSR, 3=Adem
      sumHR += sample.features[0];
      sumTemp += sample.features[1];
      sumGSR += sample.features[2];
      sumAdem += sample.features[3];
    }
  }
  
  file.close();
  
  // Bereken gemiddeldes
  if (stats.totalSamples > 0) {
    stats.avgHR = sumHR / stats.totalSamples;
    stats.avgTemp = sumTemp / stats.totalSamples;
    stats.avgGSR = sumGSR / stats.totalSamples;
    stats.avgAdem = sumAdem / stats.totalSamples;
  }
  
  Serial.printf("[PARSER] Gelezen: %d samples\n", stats.totalSamples);
  Serial.print("[PARSER] Stress level distributie: ");
  for (int i = 0; i < 7; i++) {
    if (stats.stressLevelCounts[i] > 0) {
      Serial.printf("L%d:%d ", i+1, stats.stressLevelCounts[i]);
    }
  }
  Serial.println();
  
  return stats.totalSamples > 0;
}

// ===== .CSV File Parsing (without labels) =====

bool parseCsvFile(const char* filename, std::vector<TrainingSample>& samples, DatasetStats& stats) {
  File file = SD.open(filename);
  if (!file) {
    Serial.printf("[PARSER] Fout: kan %s niet openen\n", filename);
    return false;
  }
  
  Serial.printf("[PARSER] Lezen .csv bestand: %s\n", filename);
  
  stats.filename = filename;
  stats.isLabeled = false;
  stats.totalSamples = 0;
  
  float sumHR = 0, sumTemp = 0, sumGSR = 0, sumAdem = 0;
  
  // Skip header lijn
  if (file.available()) {
    String header = file.readStringUntil('\n');
    Serial.printf("[PARSER] Header: %s\n", header.c_str());
  }
  
  // Parse data lijnen
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() < 5) continue;
    
    // Parse CSV lijn: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe (GEEN STRESS LEVEL)
    TrainingSample sample;
    int dummy_label;
    
    if (parseCsvLine(line, sample.features, dummy_label)) {
      sample.label = -1; // Geen label (nog te annoteren)
      samples.push_back(sample);
      
      stats.totalSamples++;
      sumHR += sample.features[0];
      sumTemp += sample.features[1];
      sumGSR += sample.features[2];
      sumAdem += sample.features[3];
    }
  }
  
  file.close();
  
  // Bereken gemiddeldes
  if (stats.totalSamples > 0) {
    stats.avgHR = sumHR / stats.totalSamples;
    stats.avgTemp = sumTemp / stats.totalSamples;
    stats.avgGSR = sumGSR / stats.totalSamples;
    stats.avgAdem = sumAdem / stats.totalSamples;
  }
  
  Serial.printf("[PARSER] Gelezen: %d samples (ongelabeld)\n", stats.totalSamples);
  
  return stats.totalSamples > 0;
}

// ===== Save .ALY File =====

bool saveAlyFile(const char* filename, const std::vector<TrainingSample>& samples) {
  if (samples.empty()) {
    Serial.println("[PARSER] Fout: geen samples om op te slaan");
    return false;
  }
  
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.printf("[PARSER] Fout: kan %s niet schrijven\n", filename);
    return false;
  }
  
  Serial.printf("[PARSER] Schrijven .aly bestand: %s (%d samples)\n", filename, samples.size());
  
  // Schrijf header
  file.println("Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe,StressLevel");
  
  // Schrijf data
  for (const auto& sample : samples) {
    // Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe,StressLevel
    file.print(sample.features[8], 0); file.print(",");  // Time
    file.print(sample.features[0], 1); file.print(",");  // HR
    file.print(sample.features[1], 2); file.print(",");  // Temp
    file.print(sample.features[2], 0); file.print(",");  // GSR
    file.print(sample.features[3], 1); file.print(",");  // Adem
    file.print(sample.features[4], 0); file.print(",");  // Trust (snelheid)
    file.print(sample.features[5], 0); file.print(",");  // SleevePos (positie)
    file.print(sample.features[6], 0); file.print(",");  // Suction
    file.print(sample.features[7], 0); file.print(",");  // Vibe
    file.println(sample.label);                          // StressLevel (1-7)
  }
  
  file.close();
  Serial.println("[PARSER] Bestand opgeslagen!");
  
  return true;
}

// ===== Get File Stats (quick scan) =====

bool getFileStats(const char* filename, DatasetStats& stats) {
  File file = SD.open(filename);
  if (!file) return false;
  
  stats.filename = filename;
  stats.totalSamples = 0;
  
  // Detecteer of dit .aly of .csv is
  String fn = String(filename);
  stats.isLabeled = fn.endsWith(".aly");
  
  // Skip header
  if (file.available()) {
    file.readStringUntil('\n');
  }
  
  // Tel lijnen
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 5) {
      stats.totalSamples++;
    }
  }
  
  file.close();
  return stats.totalSamples > 0;
}

// ===== Parse CSV Line =====

bool parseCsvLine(const String& line, float features[9], int& label) {
  // Parse: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe[,StressLevel]
  // Features array: [HR, Temp, GSR, Adem, Trust, SleevePos, Suction, Vibe, Time]
  // StressLevel: 1-7
  
  int commaPositions[10];
  int commaCount = 0;
  
  // Vind komma posities
  for (int i = 0; i < line.length() && commaCount < 10; i++) {
    if (line[i] == ',') {
      commaPositions[commaCount++] = i;
    }
  }
  
  // Minimaal 8 kommas voor .csv (9 kolommen), 9 kommas voor .aly (10 kolommen)
  if (commaCount < 8) {
    return false;
  }
  
  // Parse Time (eerste kolom)
  String timeStr = line.substring(0, commaPositions[0]);
  features[8] = timeStr.toFloat(); // Time op index 8
  
  // Parse HR (kolom 2)
  String hrStr = line.substring(commaPositions[0] + 1, commaPositions[1]);
  features[0] = hrStr.toFloat();
  
  // Parse Temp (kolom 3)
  String tempStr = line.substring(commaPositions[1] + 1, commaPositions[2]);
  features[1] = tempStr.toFloat();
  
  // Parse GSR (kolom 4)
  String gsrStr = line.substring(commaPositions[2] + 1, commaPositions[3]);
  features[2] = gsrStr.toFloat();
  
  // Parse Adem (kolom 5)
  String ademStr = line.substring(commaPositions[3] + 1, commaPositions[4]);
  features[3] = ademStr.toFloat();
  
  // Parse Trust (kolom 6) - bewegingssnelheid
  String trustStr = line.substring(commaPositions[4] + 1, commaPositions[5]);
  features[4] = trustStr.toFloat();
  
  // Parse SleevePos (kolom 7) - sleeve positie
  String sleeveStr = line.substring(commaPositions[5] + 1, commaPositions[6]);
  features[5] = sleeveStr.toFloat();
  
  // Parse Suction (kolom 8)
  String suctionStr = line.substring(commaPositions[6] + 1, commaPositions[7]);
  features[6] = suctionStr.toFloat();
  
  // Parse Vibe (kolom 9)
  String vibeStr = line.substring(commaPositions[7] + 1, (commaCount >= 9) ? commaPositions[8] : line.length());
  features[7] = vibeStr.toFloat();
  
  // Parse StressLevel (kolom 10, indien aanwezig)
  if (commaCount >= 9) {
    String labelStr = line.substring(commaPositions[8] + 1);
    label = labelStr.toInt();
    // Valideer: moet 1-7 zijn
    if (label < 1 || label > 7) {
      label = 3; // Default naar neutral als invalid
    }
  } else {
    label = -1; // Geen stress level (ongelabelde data)
  }
  
  return true;
}

// ===== Validate Files =====

bool validateAlyFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) return false;
  
  // Check header
  if (file.available()) {
    String header = file.readStringUntil('\n');
    header.toLowerCase();
    if (header.indexOf("stress") < 0 && header.indexOf("label") < 0) {
      file.close();
      return false; // .aly moet StressLevel of Label kolom hebben
    }
  }
  
  file.close();
  return true;
}

bool validateCsvFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) return false;
  
  // Check header
  if (file.available()) {
    String header = file.readStringUntil('\n');
    header.toLowerCase();
    // CSV moet minimaal HR, Temp, GSR hebben
    if (header.indexOf("hr") < 0 || header.indexOf("temp") < 0 || header.indexOf("gsr") < 0) {
      file.close();
      return false;
    }
  }
  
  file.close();
  return true;
}
