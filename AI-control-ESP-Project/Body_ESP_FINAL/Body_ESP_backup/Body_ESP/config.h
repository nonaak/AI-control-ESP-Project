#pragma once

/* ================================================================================
 * BODY ESP32-S3 - CONFIGURATIE BESTAND
 * ================================================================================
 * 
 * Dit bestand bevat ALLE gebruikersinstellingen voor de Body ESP.
 * Pas deze waarden aan naar jouw voorkeur.
 * 
 * Let op: Alle instellingen zijn in het Nederlands voor overzichtelijkheid.
 * ================================================================================
 */

// ================================================================================
// ANIMATIE INSTELLINGEN
// ================================================================================
// Deze instellingen bepalen hoe de grafieken op het scherm bewegen.
// Ze beïnvloeden ALLEEN de visualisatie, niet de echte machine snelheid!

// Trust (HoofdESP) sinus animatie - beweegt als een golf
#define TRUST_ANIM_SNELHEID   3.5f    // Hoe snel de golf beweegt
//#define TRUST_ANIM_SNELHEID   2.5f    // Hoe snel de golf beweegt
                                      // Lager = langzamere golf, hoger = snellere golf
                                      
#define TRUST_ANIM_HOOGTE     48.0f   // Hoe hoog de golf gaat (0-50)
                                      // 50 = volledige schermhoogte, 25 = halve hoogte

// Vibe (trillen) zaagrand animatie - ziet eruit als getande lijn
#define VIBE_ANIM_SNELHEID    0.15f   // Hoe snel de zaagrand beweegt
                                      // 0.05 = langzaam, 0.3 = snel

// ================================================================================
// SENSOR INSTELLINGEN
// ================================================================================
// Instellingen voor het uitlezen en weergeven van biometrische sensoren.

// GSR (huidgeleiding) schaling
#define GSR_SCHAAL_FACTOR     9.0f   // Schaalt GSR waarden voor grafiek weergave
//#define GSR_SCHAAL_FACTOR     10.0f
                                      // Grotere waarde = lagere pieken
                                      // Verhoog dit als GSR grafiek te hoog uitslaat

// Sensor uitlees interval
#define SENSOR_INTERVAL_MS    100     // Hoe vaak sensoren uitgelezen worden (milliseconden)
                                      // 100 = elke 0.1 sec = vloeiende grafiek
                                      // 200 = elke 0.2 sec = minder CPU gebruik maar hakkeliger

// Hartslag limieten (voor grafiek schaal)
#define HARTSLAG_MIN          50.0f   // Onderkant grafiek (BPM)
#define HARTSLAG_MAX          200.0f  // Bovenkant grafiek (BPM)

// Temperatuur limieten (voor grafiek schaal)
#define TEMPERATUUR_MIN       35.0f   // Onderkant grafiek (°C)
#define TEMPERATUUR_MAX       40.0f   // Bovenkant grafiek (°C)

// GSR maximum (voor grafiek schaal)
#define GSR_MAX               1000.0f // Bovenkant GSR grafiek

// Vacuum grafiek (zuigen modus)
#define VACUUM_MAX_MBAR       10.0f   // Bovenkant vacuum grafiek (0-10 mbar bereik)

// ================================================================================
// MULTIFUNPLAYER / HERESPHERE FUNSCRIPT
// ================================================================================

#define MFP_PC_IP "192.168.2.3"          // ← VERVANG MET JE PC IP ADRES (cmd → ipconfig)
#define MFP_PORT 8080                    // ← WEBSOCKET POORT
#define MFP_PATH "/"                     // ← WEBSOCKET PATH (meestal "/")
#define MFP_AUTO_CONNECT true            // ← AUTOMATISCH VERBINDEN (true/false)
#define MFP_RECONNECT_INTERVAL 5000      // ← HERVERBIND INTERVAL IN MS (5000 = 5 sec)
#define MFP_AI_INTEGRATIE true           // ← AI INTEGRATIE AAN/UIT (true/false)
                                         // Als true: AI kan funscript acties aanpassen o.b.v. biometrie
                                         // Als false: pure funscript zonder AI modificatie

// ================================================================================
// AI OVERRULE INSTELLINGEN
// ================================================================================
// De AI bewaakt je biometrische waardes en grijpt in bij risicosignalen.
// Het verlaagt automatisch de machine intensiteit als het te heftig wordt.

// Basis AI instellingen
#define AI_ENABLED true                  // Zet AI bescherming aan of uit
                                         // true = AI kijkt mee en beschermt je
                                         // false = geen AI interventie

// Biometrische drempelwaarden - wanneer grijpt AI in?
#define AI_HARTSLAG_LAAG      60.0f      // Als hartslag onder deze waarde: AI waarschuwing
#define AI_HARTSLAG_HOOG      140.0f     // Als hartslag boven deze waarde: AI grijpt in!
#define AI_TEMP_HOOG          38.5f      // Als temperatuur boven deze waarde: AI grijpt in!
#define AI_GSR_HOOG           800.0f     // Als GSR (stress) boven deze waarde: AI grijpt in!

// AI overrule percentages - HOE HARD grijpt AI in?
// -----------------------------------------------
// Wat gebeurt er: Bij risico verlaagt AI de machine snelheid tijdelijk.

#define AI_TRUST_REDUCTIE     0.3f       // Trust (in/uit beweging) reductie bij risico
                                         // 0.3 = verlaagt naar 70% (30% reductie)
                                         // Voorbeeld: Machine op 100% → risico → AI zet op 70%
                                         // Hogere waarde = AI remt harder af
                                         
#define AI_SLEEVE_REDUCTIE    0.4f       // Sleeve (rotatie) reductie bij risico
                                         // 0.4 = verlaagt naar 60% (40% reductie)
                                         // Voorbeeld: Rotatie op 100% → risico → AI zet op 60%
                                         // Hogere waarde = AI remt harder af
                                         
#define AI_HERSTEL_SNELHEID   0.02f      // Hoe snel bouwt AI de snelheid weer op?
                                         // 0.02 = elke 5 seconden +2% herstel
                                         // Voorbeeld: Trust op 70% → na 5sec 72% → na 10sec 74% etc.
                                         // Lagere waarde = langzamer herstel (voorzichtiger)
                                         // Hogere waarde = sneller herstel (agressiever)

// ================================================================================
// STRESS MANAGEMENT SYSTEEM
// ================================================================================
// Het systeem detecteert 7 stress levels (0-6) op basis van je biometrie.
// Deze timings bepalen hoe lang het systeem wacht voordat het van level verandert.
// Lagere levels (0-3) = langzamere reactie, hogere levels (4-6) = snellere reactie.

// Stress level timings - hoe lang moet je op een level blijven?
#define STRESS_LVL0_MINUTEN   5          // Level 0 (ontspannen): 5 minuten stabiel
#define STRESS_LVL1_MINUTEN   3          // Level 1 (rustig): 3 minuten stabiel
#define STRESS_LVL2_MINUTEN   3          // Level 2 (normaal): 3 minuten stabiel
#define STRESS_LVL3_MINUTEN   2          // Level 3 (verhoogd): 2 minuten stabiel
#define STRESS_LVL4_SECONDEN  30         // Level 4 (gestrest): 30 seconden (snellere reactie!)
#define STRESS_LVL5_SECONDEN  20         // Level 5 (zeer gestrest): 20 seconden
#define STRESS_LVL6_SECONDEN  15         // Level 6 (extreem): 15 seconden (snelste reactie!)

// Stress verandering detectie drempelwaarden (per minuut)
#define STRESS_CHANGE_RUSTIG      0.3f   // Rustige verandering
#define STRESS_CHANGE_NORMAAL     0.8f   // Normale verandering
#define STRESS_CHANGE_SNEL        1.5f   // Snelle verandering
#define STRESS_CHANGE_HEEL_SNEL   3.0f   // Heel snelle verandering

// Stress algoritme parameters
#define STRESS_HISTORY_WEIGHT     0.3f   // Gewicht van stress historie (0.0-1.0)
#define STRESS_HISTORY_WINDOW_MS  60000  // Stress historie venster in ms (60000 = 1 minuut)
#define BIO_STRESS_SENSITIVITY    1.0f   // Gevoeligheid biometrische stress detectie

// ================================================================================
// AI AUTONOMIE CONFIGURATIE
// ================================================================================
// Bepaalt hoeveel VRIJHEID de AI krijgt om zelf beslissingen te nemen.
// Hogere autonomie = AI neemt meer initiatief, lagere = AI volgt strikte regels.

// AI vrijheid level
#define AI_AUTONOMIE_LEVEL            0.3f   // Hoeveel vrijheid krijgt AI? (0.0-1.0)
                                             // 0.0 = geen autonomie, volgt alleen regels
                                             // 0.3 = 30% autonomie (default, gebalanceerd)
                                             // 1.0 = volledige autonomie, AI beslist alles
                                             
#define AI_OVERRIDE_CONFIDENCE        0.5f   // Hoe zeker moet AI zijn voor een beslissing?
                                             // 0.5 = 50% zekerheid is genoeg (default)
                                             // 0.8 = 80% zekerheid nodig (conservatiever)
                                             // Lager = AI neemt sneller beslissingen
                                             
// AI beslissingsmacht - wat MAG de AI?
#define AI_KAN_LEVELS_OVERSLAAN       false   // Mag AI stress levels overslaan?
//#define AI_KAN_LEVELS_OVERSLAAN       true
                                             // true = AI kan van level 2 naar 5 springen
                                             // false = AI moet elk level doorlopen
                                             
#define AI_KAN_TIMERS_NEGEREN         true   // Mag AI de stress timers negeren?
                                             // true = AI kan sneller reageren dan timers
                                             // false = AI moet timers respecteren
                                             
#define AI_KAN_EMERGENCY_OVERRIDE     false   // Mag AI noodstop beslissingen nemen?
//#define AI_KAN_EMERGENCY_OVERRIDE     true
                                             // true = AI kan direct ingrijpen bij gevaar
                                             // false = AI volgt normale procedures

// AI leren en aanpassen
#define AI_LEER_SNELHEID              0.1f   // Hoe snel AI zich aanpast (0.0-1.0)
#define AI_MIN_SESSIES_VOOR_AUTONOMIE 0      // Minimum sessies voordat autonomie actief wordt
#define AI_USER_FEEDBACK_WEIGHT       0.8f   // Gewicht van gebruiker feedback (0.0-1.0)

// AI updates
#define AI_STRESS_ENABLED         true       // AI stress voorspelling aan/uit
#define AI_CONFIDENCE_THRESHOLD   0.7f       // Minimale AI confidence voor acties
#define AI_UPDATE_INTERVAL_MS     5000       // AI update interval in milliseconden
#define AI_TRAINING_MODE          false      // AI training data collection mode
#define AUTO_RECORD_SESSIONS      true       // Automatisch CSV bestanden opnemen voor sessies

// ================================================================================
// KALIBRATIE
// ================================================================================
// Kalibratie gebeurt via het menu systeem:
// Menu → Kalibratie → Auto Kalibreer Alles (10 seconden)
// Waardes worden opgeslagen in ESP32 EEPROM en blijven bewaard na reboot.
