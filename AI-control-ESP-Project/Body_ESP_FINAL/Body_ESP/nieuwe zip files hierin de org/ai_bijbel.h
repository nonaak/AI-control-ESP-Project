/*
 * AI BIJBEL - Hardcoded AI Bevoegdheid Schaal
 * 
 * Dit bestand bevat ALLE regels voor AI controle.
 * 10 niveaus van 0% (AI uit) tot 100% (AI totale controle)
 * 
 * NUNCHUK HEEFT ALTIJD PRIORITEIT - DIT IS WET!
 */

#ifndef AI_BIJBEL_H
#define AI_BIJBEL_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         LEVEL SYSTEEM (0-7)
// ═══════════════════════════════════════════════════════════════════════════

#define LEVEL_RECOVERY      0   // Minimale beweging (recovery/cooldown)
#define LEVEL_VERY_SLOW     1   // Zeer langzaam
#define LEVEL_SLOW          2   // Langzaam
#define LEVEL_MEDIUM_SLOW   3   // Medium langzaam
#define LEVEL_MEDIUM        4   // Medium
#define LEVEL_MEDIUM_FAST   5   // Medium snel
#define LEVEL_FAST          6   // Snel
#define LEVEL_EDGE_ZONE     7   // Maximum snelheid (EDGE ZONE + Orgasme trigger)

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BEVOEGDHEID CONFIGURATIE
// ═══════════════════════════════════════════════════════════════════════════

struct AIBevoegdheidConfig {
  uint8_t autonomyPercent;      // 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
  
  // Level controle
  int8_t minLevel;              // Minimum level AI mag gebruiken
  int8_t maxLevelPermanent;     // Maximum level AI permanent mag gebruiken
  int8_t maxLevelTijdelijk;     // Maximum level voor tijdelijke dip (-1 = geen)
  uint32_t tijdelijkeDipMs;     // Hoe lang mag tijdelijke dip duren (ms)
  
  // Hardware controle
  bool magVibeControleren;      // Mag AI vibe aan/uit zetten?
  bool magZuigControleren;      // Mag AI zuig aan/uit zetten?
  
  // Edge/Orgasme controle
  bool magNaarLevel7;           // Mag AI naar Level 7 (edge zone)?
  int8_t maxEdgesMomentjes;     // Max aantal edges per sessie (-1 = onbeperkt)
  bool magOrgasmeTrigger;       // Mag AI orgasme triggeren?
  
  // Beschrijving
  const char* beschrijving;
};

// ═══════════════════════════════════════════════════════════════════════════
//                    HARDCODED AI BIJBEL (10 NIVEAUS)
// ═══════════════════════════════════════════════════════════════════════════

static const AIBevoegdheidConfig AI_BIJBEL[] = {
  // 0% - AI UIT (ALLEEN OBSERVEREN)
  {
    .autonomyPercent = 0,
    .minLevel = -1, .maxLevelPermanent = -1, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = false, .magZuigControleren = false,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "AI UIT - Alleen observeren en leren"
  },
  
  // 10% - AI OBSERVEERT EN LEERT
  {
    .autonomyPercent = 10,
    .minLevel = -1, .maxLevelPermanent = -1, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = false, .magZuigControleren = false,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "AI observeert en leert, geen controle"
  },
  
  // 20% - AI SUBTIELE AANPASSINGEN
  {
    .autonomyPercent = 20,
    .minLevel = 0, .maxLevelPermanent = 2, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = false, .magZuigControleren = false,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "Subtiele aanpassingen (Level 0-2)"
  },
  
  // 30% - AI KLEINE LEVEL CONTROLE
  {
    .autonomyPercent = 30,
    .minLevel = 0, .maxLevelPermanent = 3, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "Kleine controle (Level 0-3) + Vibe/Zuig"
  },
  
  // 40% - AI MEDIUM LEVEL CONTROLE
  {
    .autonomyPercent = 40,
    .minLevel = 0, .maxLevelPermanent = 4, .maxLevelTijdelijk = 5, .tijdelijkeDipMs = 30000,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "Medium controle (Level 0-4, tijdelijk 5)"
  },
  
  // 50% - AI MEDE-BESLISSER (50/50 CONTROLE)
  {
    .autonomyPercent = 50,
    .minLevel = 0, .maxLevelPermanent = 5, .maxLevelTijdelijk = 6, .tijdelijkeDipMs = 45000,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "Mede-beslisser (Level 0-5, tijdelijk 6)"
  },
  
  // 60% - AI MEERDERHEID
  {
    .autonomyPercent = 60,
    .minLevel = 0, .maxLevelPermanent = 6, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = false, .maxEdgesMomentjes = 0, .magOrgasmeTrigger = false,
    .beschrijving = "AI meerderheid (Level 0-6 volledig)"
  },
  
  // 70% - AI DOMINANT (EDGE EXPERIMENTEN)
  {
    .autonomyPercent = 70,
    .minLevel = 0, .maxLevelPermanent = 7, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = true, .maxEdgesMomentjes = 3, .magOrgasmeTrigger = false,
    .beschrijving = "AI dominant (Level 0-7, max 3 edges)"
  },
  
  // 80% - AI VOLLEDIGE CONTROLE
  {
    .autonomyPercent = 80,
    .minLevel = 0, .maxLevelPermanent = 7, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = true, .maxEdgesMomentjes = 5, .magOrgasmeTrigger = false,
    .beschrijving = "AI volledige controle (max 5 edges)"
  },
  
  // 90% - AI MASTER (ORGASME TRIGGER ACTIEF)
  {
    .autonomyPercent = 90,
    .minLevel = 0, .maxLevelPermanent = 7, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = true, .maxEdgesMomentjes = -1, .magOrgasmeTrigger = true,
    .beschrijving = "AI Master (onbeperkt edges, mag orgasme triggeren)"
  },
  
  // 100% - AI TOTAL CONTROL
  {
    .autonomyPercent = 100,
    .minLevel = 0, .maxLevelPermanent = 7, .maxLevelTijdelijk = -1, .tijdelijkeDipMs = 0,
    .magVibeControleren = true, .magZuigControleren = true,
    .magNaarLevel7 = true, .maxEdgesMomentjes = -1, .magOrgasmeTrigger = true,
    .beschrijving = "AI TOTALE controle (absolute vrijheid)"
  }
};

#define AI_BIJBEL_LEVELS 11

// ═══════════════════════════════════════════════════════════════════════════
//                         RUNTIME STATE
// ═══════════════════════════════════════════════════════════════════════════

struct AIRuntimeState {
  uint8_t currentAutonomy;          // Huidige autonomy % (0-100)
  int8_t currentLevel;              // Huidig actief level (0-7)
  
  // Tijdelijke dip tracking
  bool inTijdelijkeDip;             // Zijn we in een tijdelijke dip?
  uint32_t tijdelijkeDipStart;      // Wanneer begon de dip?
  int8_t tijdelijkeDipLevel;        // Welk level is de dip?
  
  // Edge tracking
  int edgeCount;                    // Aantal edges deze sessie
  uint32_t lastEdgeTime;            // Laatste edge timestamp
  
  // Sessie state
  bool sessionActive;               // Is er een actieve sessie?
  bool isPaused;                    // Emergency pause actief?
  bool orgasmeTriggered;            // Is orgasme getriggerd?
  
  // Nunchuk override tracking
  uint32_t lastNunchukOverride;     // Laatste nunchuk input timestamp
  bool nunchukOverrideActive;       // Is nunchuk momenteel aan het overriden?
};

// Global runtime state
extern AIRuntimeState aiState;

// ═══════════════════════════════════════════════════════════════════════════
//                         HELPER FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Krijg config voor een autonomy percentage (rondt af naar dichtstbijzijnde niveau)
inline const AIBevoegdheidConfig* aiBijbel_getConfig(uint8_t autonomyPercent) {
  // Rond af naar dichtstbijzijnde 10%
  int index = (autonomyPercent + 5) / 10;
  if (index < 0) index = 0;
  if (index >= AI_BIJBEL_LEVELS) index = AI_BIJBEL_LEVELS - 1;
  return &AI_BIJBEL[index];
}

// Check of AI naar een bepaald level mag
inline bool aiBijbel_magNaarLevel(uint8_t autonomyPercent, int8_t targetLevel) {
  const AIBevoegdheidConfig* config = aiBijbel_getConfig(autonomyPercent);
  
  // AI heeft geen controle?
  if (config->maxLevelPermanent < 0) return false;
  
  // Binnen permanent bereik?
  if (targetLevel <= config->maxLevelPermanent) return true;
  
  // Tijdelijk toegestaan?
  if (config->maxLevelTijdelijk >= 0 && targetLevel <= config->maxLevelTijdelijk) {
    return true;  // Caller moet tijdelijke dip timer bijhouden!
  }
  
  return false;
}

// Check of AI vibe mag controleren
inline bool aiBijbel_magVibe(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->magVibeControleren;
}

// Check of AI zuig mag controleren
inline bool aiBijbel_magZuig(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->magZuigControleren;
}

// Check of AI naar Level 7 (edge zone) mag
inline bool aiBijbel_magEdgeZone(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->magNaarLevel7;
}

// Check of AI orgasme mag triggeren
inline bool aiBijbel_magOrgasmeTrigger(uint8_t autonomyPercent) {
  // Code threshold is 85%, maar bijbel zegt 90%+
  // We volgen de bijbel: alleen bij 90%+
  return aiBijbel_getConfig(autonomyPercent)->magOrgasmeTrigger;
}

// Krijg max edges voor dit niveau (-1 = onbeperkt)
inline int8_t aiBijbel_getMaxEdges(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->maxEdgesMomentjes;
}

// Check of we nog een edge mogen doen
inline bool aiBijbel_magNogEenEdge(uint8_t autonomyPercent, int currentEdgeCount) {
  int8_t maxEdges = aiBijbel_getMaxEdges(autonomyPercent);
  if (maxEdges < 0) return true;  // Onbeperkt
  return currentEdgeCount < maxEdges;
}

// Krijg tijdelijke dip duur in ms (0 = geen tijdelijke dips)
inline uint32_t aiBijbel_getTijdelijkeDipDuur(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->tijdelijkeDipMs;
}

// Krijg beschrijving voor display
inline const char* aiBijbel_getBeschrijving(uint8_t autonomyPercent) {
  return aiBijbel_getConfig(autonomyPercent)->beschrijving;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    LEVEL -> SPEED CONVERSIE
// ═══════════════════════════════════════════════════════════════════════════

// Converteer level (0-7) naar Keon speed percentage (0-100%)
inline uint8_t aiBijbel_levelToSpeed(int8_t level) {
  // Lineaire mapping: Level 0 = 10%, Level 7 = 100%
  if (level < 0) level = 0;
  if (level > 7) level = 7;
  
  // Level 0 = 10%, Level 1 = 22%, ..., Level 7 = 100%
  return 10 + (level * 90 / 7);
}

// Converteer speed (0-100%) terug naar level (0-7)
inline int8_t aiBijbel_speedToLevel(uint8_t speedPercent) {
  if (speedPercent <= 10) return 0;
  if (speedPercent >= 100) return 7;
  
  // Inverse van bovenstaande formule
  return (int8_t)((speedPercent - 10) * 7 / 90);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    STRESS LEVEL -> AI LEVEL MAPPING
// ═══════════════════════════════════════════════════════════════════════════

// Converteer ML stress level (0-7) naar AI actie level
// Dit bepaalt hoe AI reageert op gemeten stress
inline int8_t aiBijbel_stressToActionLevel(int8_t stressLevel, uint8_t autonomyPercent) {
  const AIBevoegdheidConfig* config = aiBijbel_getConfig(autonomyPercent);
  
  // Geen AI controle?
  if (config->maxLevelPermanent < 0) return -1;
  
  // Stress level 0-1: Rustig → Lage speed (Level 0-2)
  // Stress level 2-3: Normaal → Medium speed (Level 3-4)
  // Stress level 4-5: Verhoogd → Hogere speed (Level 5-6)
  // Stress level 6-7: Hoog/Edge → Max speed (Level 6-7)
  
  int8_t actionLevel;
  
  switch (stressLevel) {
    case 0: actionLevel = 0; break;  // Ontspannen → Recovery
    case 1: actionLevel = 1; break;  // Rustig → Zeer langzaam
    case 2: actionLevel = 2; break;  // Normaal → Langzaam
    case 3: actionLevel = 3; break;  // Licht verhoogd → Medium langzaam
    case 4: actionLevel = 4; break;  // Verhoogd → Medium
    case 5: actionLevel = 5; break;  // Gestrest → Medium snel
    case 6: actionLevel = 6; break;  // Zeer gestrest → Snel
    case 7: actionLevel = 7; break;  // Extreem → Edge zone
    default: actionLevel = 3; break; // Default naar medium
  }
  
  // Beperk tot wat AI mag
  if (actionLevel > config->maxLevelPermanent) {
    // Check tijdelijke dip
    if (config->maxLevelTijdelijk >= 0 && actionLevel <= config->maxLevelTijdelijk) {
      // Tijdelijk toegestaan - caller moet timer bijhouden
      return actionLevel;
    }
    // Beperk tot max permanent
    actionLevel = config->maxLevelPermanent;
  }
  
  return actionLevel;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         DEBUG OUTPUT
// ═══════════════════════════════════════════════════════════════════════════

inline void aiBijbel_printConfig(uint8_t autonomyPercent) {
  const AIBevoegdheidConfig* config = aiBijbel_getConfig(autonomyPercent);
  
  Serial.println("═══════════════════════════════════════════════");
  Serial.printf("AI BIJBEL CONFIG: %d%%\n", config->autonomyPercent);
  Serial.println("═══════════════════════════════════════════════");
  Serial.printf("Beschrijving: %s\n", config->beschrijving);
  Serial.printf("Level bereik: %d - %d (permanent)\n", config->minLevel, config->maxLevelPermanent);
  if (config->maxLevelTijdelijk >= 0) {
    Serial.printf("Tijdelijk tot: Level %d (%d sec)\n", config->maxLevelTijdelijk, config->tijdelijkeDipMs / 1000);
  }
  Serial.printf("Vibe controle: %s\n", config->magVibeControleren ? "JA" : "NEE");
  Serial.printf("Zuig controle: %s\n", config->magZuigControleren ? "JA" : "NEE");
  Serial.printf("Level 7 (Edge): %s\n", config->magNaarLevel7 ? "JA" : "NEE");
  Serial.printf("Max edges: %s\n", config->maxEdgesMomentjes < 0 ? "Onbeperkt" : String(config->maxEdgesMomentjes).c_str());
  Serial.printf("Orgasme trigger: %s\n", config->magOrgasmeTrigger ? "JA" : "NEE");
  Serial.println("═══════════════════════════════════════════════");
}

#endif // AI_BIJBEL_H
