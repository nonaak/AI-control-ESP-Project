/*
  ML Decision Tree Implementation
  
  ID3 algoritme voor classification op sensor data
*/

#include "ml_decision_tree.h"
#include <cmath>
#include <algorithm>

// ===== DecisionTree Implementation =====

DecisionTree::DecisionTree(int maxDepth, int minSamplesLeaf) 
  : root(nullptr), maxDepth(maxDepth), minSamplesLeaf(minSamplesLeaf) {
}

DecisionTree::~DecisionTree() {
  clear();
}

void DecisionTree::clear() {
  if (root) {
    delete root;
    root = nullptr;
  }
}

// ===== Training =====

bool DecisionTree::train(const std::vector<TrainingSample>& trainingData) {
  if (trainingData.empty()) {
    Serial.println("[DT] Fout: geen training data");
    return false;
  }
  
  Serial.printf("[DT] Start training met %d samples...\n", trainingData.size());
  
  clear();
  root = buildTree(trainingData, 0);
  
  if (root) {
    Serial.println("[DT] Training voltooid!");
    return true;
  } else {
    Serial.println("[DT] Fout: training mislukt");
    return false;
  }
}

DecisionNode* DecisionTree::buildTree(const std::vector<TrainingSample>& samples, int depth) {
  // Stop condities
  if (samples.empty()) return nullptr;
  
  // Check of alle samples dezelfde label hebben (pure node)
  bool allSame = true;
  int firstLabel = samples[0].label;
  for (size_t i = 1; i < samples.size(); i++) {
    if (samples[i].label != firstLabel) {
      allSame = false;
      break;
    }
  }
  
  // Maak leaf node als pure, te diep, of te weinig samples
  if (allSame || depth >= maxDepth || samples.size() < (size_t)(minSamplesLeaf * 2)) {
    DecisionNode* leaf = new DecisionNode();
    leaf->isLeaf = true;
    leaf->label = getMajorityLabel(samples);
    return leaf;
  }
  
  // Vind beste split
  int bestFeature = -1;
  float bestThreshold = 0.0f;
  findBestSplit(samples, bestFeature, bestThreshold);
  
  if (bestFeature == -1) {
    // Geen goede split gevonden, maak leaf
    DecisionNode* leaf = new DecisionNode();
    leaf->isLeaf = true;
    leaf->label = getMajorityLabel(samples);
    return leaf;
  }
  
  // Split data
  std::vector<TrainingSample> leftSamples, rightSamples;
  for (const auto& sample : samples) {
    if (sample.features[bestFeature] <= bestThreshold) {
      leftSamples.push_back(sample);
    } else {
      rightSamples.push_back(sample);
    }
  }
  
  // Check of split valid is
  if (leftSamples.empty() || rightSamples.empty()) {
    DecisionNode* leaf = new DecisionNode();
    leaf->isLeaf = true;
    leaf->label = getMajorityLabel(samples);
    return leaf;
  }
  
  // Maak decision node en recursief bouwen
  DecisionNode* node = new DecisionNode();
  node->isLeaf = false;
  node->featureIndex = bestFeature;
  node->threshold = bestThreshold;
  node->left = buildTree(leftSamples, depth + 1);
  node->right = buildTree(rightSamples, depth + 1);
  
  return node;
}

void DecisionTree::findBestSplit(const std::vector<TrainingSample>& samples, int& bestFeature, float& bestThreshold) {
  bestFeature = -1;
  bestThreshold = 0.0f;
  float bestGain = -1.0f;
  
  // Probeer elke feature
  for (int featureIdx = 0; featureIdx < 9; featureIdx++) {
    // Verzamel alle unieke waarden voor deze feature
    std::vector<float> values;
    for (const auto& sample : samples) {
      values.push_back(sample.features[featureIdx]);
    }
    std::sort(values.begin(), values.end());
    
    // Probeer splits tussen opeenvolgende unieke waarden
    for (size_t i = 0; i < values.size() - 1; i++) {
      if (values[i] == values[i+1]) continue; // Skip duplicaten
      
      float threshold = (values[i] + values[i+1]) / 2.0f;
      float gain = calculateInformationGain(samples, featureIdx, threshold);
      
      if (gain > bestGain) {
        bestGain = gain;
        bestFeature = featureIdx;
        bestThreshold = threshold;
      }
    }
  }
}

float DecisionTree::calculateInformationGain(const std::vector<TrainingSample>& samples, int featureIdx, float threshold) {
  // Split samples
  std::vector<TrainingSample> leftSamples, rightSamples;
  for (const auto& sample : samples) {
    if (sample.features[featureIdx] <= threshold) {
      leftSamples.push_back(sample);
    } else {
      rightSamples.push_back(sample);
    }
  }
  
  if (leftSamples.empty() || rightSamples.empty()) {
    return 0.0f; // Geen gain als split leeg is
  }
  
  // Bereken entropies
  float parentEntropy = calculateEntropy(samples);
  float leftEntropy = calculateEntropy(leftSamples);
  float rightEntropy = calculateEntropy(rightSamples);
  
  // Weighted average entropy na split
  float n = samples.size();
  float nLeft = leftSamples.size();
  float nRight = rightSamples.size();
  float childEntropy = (nLeft / n) * leftEntropy + (nRight / n) * rightEntropy;
  
  return parentEntropy - childEntropy;
}

float DecisionTree::calculateEntropy(const std::vector<TrainingSample>& samples) {
  if (samples.empty()) return 0.0f;
  
  // Tel labels
  int labelCounts[7] = {0}; // 7 stress levels (1-7)
  for (const auto& sample : samples) {
    if (sample.label >= 1 && sample.label <= 7) {
      labelCounts[sample.label - 1]++;
    }
  }
  
  // Bereken entropy: -sum(p * log2(p))
  float entropy = 0.0f;
  float n = samples.size();
  for (int i = 0; i < 7; i++) {
    if (labelCounts[i] > 0) {
      float p = labelCounts[i] / n;
      entropy -= p * log2(p);
    }
  }
  
  return entropy;
}

int DecisionTree::getMajorityLabel(const std::vector<TrainingSample>& samples) {
  if (samples.empty()) return 3; // Default: neutral stress
  
  int labelCounts[7] = {0};
  for (const auto& sample : samples) {
    if (sample.label >= 1 && sample.label <= 7) {
      labelCounts[sample.label - 1]++;
    }
  }
  
  int maxCount = 0;
  int majorityLabel = 3; // Default: neutral stress
  for (int i = 0; i < 7; i++) {
    if (labelCounts[i] > maxCount) {
      maxCount = labelCounts[i];
      majorityLabel = i + 1; // Labels zijn 1-7
    }
  }
  
  return majorityLabel;
}

// ===== Prediction =====

int DecisionTree::predict(const float features[9]) {
  if (!root) {
    Serial.println("[DT] Fout: geen model geladen");
    return -1;
  }
  
  return predictNode(root, features);
}

int DecisionTree::predictNode(const DecisionNode* node, const float features[9]) {
  if (!node) return -1;
  
  if (node->isLeaf) {
    return node->label;
  }
  
  // Volg decision path
  if (features[node->featureIndex] <= node->threshold) {
    return predictNode(node->left, features);
  } else {
    return predictNode(node->right, features);
  }
}

// ===== Model Persistence =====

String DecisionTree::serialize() {
  if (!root) return "{}";
  
  String json = "";
  serializeNode(root, json, 0);
  return json;
}

void DecisionTree::serializeNode(const DecisionNode* node, String& json, int depth) {
  if (!node) {
    json += "null";
    return;
  }
  
  json += "{";
  json += "\"leaf\":";
  json += node->isLeaf ? "true" : "false";
  
  if (node->isLeaf) {
    json += ",\"label\":";
    json += String(node->label);
  } else {
    json += ",\"feature\":";
    json += String(node->featureIndex);
    json += ",\"threshold\":";
    json += String(node->threshold, 4);
    json += ",\"left\":";
    serializeNode(node->left, json, depth + 1);
    json += ",\"right\":";
    serializeNode(node->right, json, depth + 1);
  }
  
  json += "}";
}

bool DecisionTree::deserialize(const String& json) {
  clear();
  
  if (json.length() < 3) {
    Serial.println("[DT] Fout: ongeldig JSON formaat");
    return false;
  }
  
  int pos = 0;
  root = deserializeNode(json, pos);
  
  return root != nullptr;
}

DecisionNode* DecisionTree::deserializeNode(const String& json, int& pos) {
  // Skip whitespace
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
    pos++;
  }
  
  if (pos >= json.length()) return nullptr;
  
  // Check voor null
  if (json.substring(pos, pos + 4) == "null") {
    pos += 4;
    return nullptr;
  }
  
  // Verwacht '{'
  if (json[pos] != '{') return nullptr;
  pos++;
  
  DecisionNode* node = new DecisionNode();
  
  // Parse node properties
  while (pos < json.length() && json[pos] != '}') {
    // Skip whitespace en kommas
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n')) {
      pos++;
    }
    
    if (json[pos] == '}') break;
    
    // Read key
    if (json[pos] != '"') {
      delete node;
      return nullptr;
    }
    pos++; // Skip opening "
    
    int keyStart = pos;
    while (pos < json.length() && json[pos] != '"') pos++;
    String key = json.substring(keyStart, pos);
    pos++; // Skip closing "
    
    // Skip ':' en whitespace
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ')) pos++;
    
    // Read value based on key
    if (key == "leaf") {
      node->isLeaf = json.substring(pos, pos + 4) == "true";
      pos += (node->isLeaf ? 4 : 5);
    } else if (key == "label") {
      int numStart = pos;
      while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) pos++;
      node->label = json.substring(numStart, pos).toInt();
    } else if (key == "feature") {
      int numStart = pos;
      while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) pos++;
      node->featureIndex = json.substring(numStart, pos).toInt();
    } else if (key == "threshold") {
      int numStart = pos;
      while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) pos++;
      node->threshold = json.substring(numStart, pos).toFloat();
    } else if (key == "left") {
      node->left = deserializeNode(json, pos);
    } else if (key == "right") {
      node->right = deserializeNode(json, pos);
    }
  }
  
  if (pos < json.length() && json[pos] == '}') {
    pos++;
  }
  
  return node;
}

// ===== Helper Functions =====

const char* getFeatureName(int index) {
  const char* names[] = {
    "HR", "Temp", "GSR", "Adem", 
    "Trust", "SleevePos", "Suction", "Vibe", "Time"
  };
  if (index >= 0 && index < 9) {
    return names[index];
  }
  return "Unknown";
}

void normalizeFeatures(float features[9]) {
  features[0] = normalizeHR(features[0]);
  features[1] = normalizeTemp(features[1]);
  features[2] = normalizeGSR(features[2]);
  features[3] = normalizeAdem(features[3]);
  // Trust (snelheid), SleevePos (positie), Suction, Vibe zijn 0-100 of 0-255, normaliseer naar 0-1
  for (int i = 4; i < 8; i++) {
    features[i] = constrain(features[i], 0.0f, 255.0f) / 255.0f;
  }
  // Time normaliseren is lastig, laat op ruwe milliseconden (of gebruik modulo voor time-of-day)
  // Voor nu: laat Time zoals het is of gebruik als relatieve feature
}

float normalizeHR(float hr) {
  // 40-200 BPM -> 0-1
  return constrain((hr - 40.0f) / 160.0f, 0.0f, 1.0f);
}

float normalizeTemp(float temp) {
  // 30-40°C -> 0-1
  return constrain((temp - 30.0f) / 10.0f, 0.0f, 1.0f);
}

float normalizeGSR(float gsr) {
  // 0-4095 -> 0-1
  return constrain(gsr / 4095.0f, 0.0f, 1.0f);
}

float normalizeAdem(float adem) {
  // 0-100% -> 0-1
  return constrain(adem / 100.0f, 0.0f, 1.0f);
}
