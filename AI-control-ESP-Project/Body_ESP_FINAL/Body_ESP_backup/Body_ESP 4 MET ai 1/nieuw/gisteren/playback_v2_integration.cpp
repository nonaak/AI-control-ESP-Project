/*
  PLAYBACK SCREEN V2 - INTEGRATIE VOORBEELD
  
  ═══════════════════════════════════════════════════════════════════════════
  Dit bestand toont hoe je de nieuwe PlaybackScreenV2 integreert in body_menu.cpp
  ═══════════════════════════════════════════════════════════════════════════
*/

// ═══════════════════════════════════════════════════════════════════════════
//                    1. INCLUDE TOEVOEGEN (bovenaan body_menu.cpp)
// ═══════════════════════════════════════════════════════════════════════════

#include "playback_screen_v2.h"
#include "ml_annotation.h"

// ═══════════════════════════════════════════════════════════════════════════
//                    2. IN SETUP (bij bodyMenuSetup of begin)
// ═══════════════════════════════════════════════════════════════════════════

void initPlaybackScreen() {
  playbackScreen.begin(body_gfx);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    3. VERVANG drawPlaybackScreen() MET DIT:
// ═══════════════════════════════════════════════════════════════════════════

void drawPlaybackScreen_NEW() {
  // Update state
  playbackScreen.setFilename(selectedPlaybackFile);
  playbackScreen.setProgress(playbackTimestamp, playbackTotalTime);
  playbackScreen.setSpeed(playbackSpeed);
  playbackScreen.setPaused(isPlaybackPaused);
  
  // Update levels
  playbackScreen.setCurrentLevel(playbackCurrentStressLevel);  // Moet je bijhouden
  playbackScreen.setAIPrediction(playbackLastAILevel);
  
  // Check of er een user annotatie is op deze timestamp
  int userLevel = ml_getAnnotationAt(playbackTimestamp);
  if (userLevel >= 0) {
    playbackScreen.setUserAnnotation(userLevel);
  } else {
    playbackScreen.setUserAnnotation(-1);  // Geen annotatie
  }
  
  // Update sensors
  playbackScreen.setSensorValues(playbackLastHR, playbackLastTemp, playbackLastGSR);
  
  // Push level sample voor grafiek
  playbackScreen.pushLevelSample(playbackCurrentStressLevel);
  
  // Teken
  playbackScreen.drawDynamicElements();
}

// ═══════════════════════════════════════════════════════════════════════════
//                    4. BIJ PLAYBACK START - LAAD MARKERS
// ═══════════════════════════════════════════════════════════════════════════

void loadPlaybackMarkers() {
  playbackScreen.clearMarkers();
  
  // Laad AI markers uit .anl bestand
  String anlFilename = String(selectedPlaybackFile);
  anlFilename.replace(".csv", ".anl");
  
  File anlFile = SD_MMC.open(("/recordings/" + anlFilename).c_str(), FILE_READ);
  if (anlFile) {
    // Skip header
    anlFile.readStringUntil('\n');
    
    int lineNum = 0;
    while (anlFile.available() && lineNum < MAX_VISIBLE_MARKERS) {
      String line = anlFile.readStringUntil('\n');
      
      // Parse timestamp (eerste kolom) en StressLevel (laatste kolom)
      int firstComma = line.indexOf(',');
      int lastComma = line.lastIndexOf(',');
      
      if (firstComma > 0 && lastComma > firstComma) {
        float timestamp = line.substring(0, firstComma).toFloat();
        int level = line.substring(lastComma + 1).toInt();
        
        // Voeg marker toe (sample elke 10 regels om niet te vol te worden)
        if (lineNum % 10 == 0) {
          playbackScreen.addMarker(timestamp, level, true, false);
        }
      }
      
      lineNum++;
    }
    anlFile.close();
  }
  
  // Laad user annotaties
  String annFilename = String(selectedPlaybackFile);
  annFilename.replace(".csv", ".ann");
  annFilename.replace(".anl", ".ann");
  
  File annFile = SD_MMC.open(("/recordings/" + annFilename).c_str(), FILE_READ);
  if (annFile) {
    annFile.readStringUntil('\n');  // Skip header
    
    while (annFile.available()) {
      String line = annFile.readStringUntil('\n');
      
      // Parse: Timestamp,Line,HR,Temp,GSR,AI_Level,User_Level,...
      int firstComma = line.indexOf(',');
      if (firstComma > 0) {
        float timestamp = line.substring(0, firstComma).toFloat();
        
        // Vind user level (7e kolom)
        int commaCount = 0;
        int startIdx = 0;
        for (int i = 0; i < line.length(); i++) {
          if (line.charAt(i) == ',') {
            commaCount++;
            if (commaCount == 6) startIdx = i + 1;
            if (commaCount == 7) {
              int userLevel = line.substring(startIdx, i).toInt();
              playbackScreen.addMarker(timestamp, userLevel, false, false);
              break;
            }
          }
        }
      }
    }
    annFile.close();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                    5. TOUCH HANDLING VOOR NIEUWE KNOPPEN
// ═══════════════════════════════════════════════════════════════════════════

void handlePlaybackTouch(int x, int y) {
  int result = playbackScreen.handleTouch(x, y);
  
  switch (result) {
    case 0:  // STOP
      stopPlayback();
      break;
      
    case 1:  // PLAY/PAUZE
      isPlaybackPaused = !isPlaybackPaused;
      break;
      
    case 2:  // -10s
      seekPlayback(-10);
      break;
      
    case 3:  // +10s
      seekPlayback(+10);
      break;
      
    case 4:  // AI-ACTIE
      openStressPopup();
      break;
      
    case 10:  // Speed down
      playbackSpeed = max(10.0f, playbackSpeed - 10);
      break;
      
    case 11:  // Speed up
      playbackSpeed = min(200.0f, playbackSpeed + 10);
      break;
      
    case 20:  // Seek (click op progress bar)
      {
        float seekRatio = (float)(x - PB_PROGRESS_X) / PB_PROGRESS_W;
        seekRatio = constrain(seekRatio, 0.0f, 1.0f);
        float seekTime = seekRatio * playbackTotalTime;
        seekToTime(seekTime);
      }
      break;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                    6. ENCODER NAVIGATIE VOOR NIEUWE KNOPPEN
// ═══════════════════════════════════════════════════════════════════════════

void handlePlaybackEncoder(int direction) {
  int current = playbackScreen.getSelectedButton();
  int buttonCount = playbackScreen.getButtonCount();
  
  if (direction > 0) {
    current = (current + 1) % buttonCount;
  } else {
    current = (current - 1 + buttonCount) % buttonCount;
  }
  
  playbackScreen.setSelectedButton(current);
}

void handlePlaybackEncoderPress() {
  int selected = playbackScreen.getSelectedButton();
  
  switch (selected) {
    case 0:  // STOP
      stopPlayback();
      break;
    case 1:  // PLAY/PAUZE
      isPlaybackPaused = !isPlaybackPaused;
      break;
    case 2:  // -10s
      seekPlayback(-10);
      break;
    case 3:  // +10s
      seekPlayback(+10);
      break;
    case 4:  // AI-ACTIE
      openStressPopup();
      break;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                    7. HELPER FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

void seekPlayback(int seconds) {
  // Bereken nieuwe positie
  float newTime = playbackTimestamp + seconds;
  newTime = constrain(newTime, 0, playbackTotalTime);
  
  seekToTime(newTime);
}

void seekToTime(float targetTime) {
  // Heropen bestand en seek naar positie
  // Dit is een vereenvoudigde versie - je moet dit aanpassen voor je CSV parsing
  
  if (!playbackFile) return;
  
  // Schat regel nummer (aanname: ~10 samples per seconde)
  int targetLine = (int)(targetTime * 10);
  
  playbackFile.seek(0);
  playbackFile.readStringUntil('\n');  // Skip header
  
  for (int i = 0; i < targetLine && playbackFile.available(); i++) {
    playbackFile.readStringUntil('\n');
  }
  
  playbackCurrentLine = targetLine;
  playbackTimestamp = targetTime;
}

void openStressPopup() {
  // Pauzeer playback
  isPlaybackPaused = true;
  g4_pauseRendering = true;
  
  // Open annotatie bestand
  ml_openAnnotationFile(String(selectedPlaybackFile));
  
  // Check bestaande annotatie
  int existingLevel = ml_getAnnotationAt(playbackTimestamp);
  if (existingLevel >= 0) {
    selectedStressLevel = existingLevel;
  } else if (playbackLastAILevel >= 0) {
    selectedStressLevel = playbackLastAILevel;
  } else {
    selectedStressLevel = playbackCurrentStressLevel;
  }
  
  stressPopupActive = true;
  stressLevelConfirmed = false;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    8. VARIABELEN DIE JE MOET TOEVOEGEN
// ═══════════════════════════════════════════════════════════════════════════

/*
Voeg deze variabelen toe aan je playback state:

static float playbackTotalTime = 0;        // Totale tijd van recording in seconden
static int playbackCurrentStressLevel = 0; // Huidig berekend stress level (0-7)

En update playbackCurrentStressLevel in je updatePlayback() functie:
  // Bereken stress level op basis van sensors
  playbackCurrentStressLevel = calculateStressLevel(playbackLastHR, playbackLastTemp, playbackLastGSR);
*/

// ═══════════════════════════════════════════════════════════════════════════
//                    SAMENVATTING
// ═══════════════════════════════════════════════════════════════════════════

/*
Het nieuwe playback scherm biedt:

1. GROOT LEVEL PANEEL (links)
   - Groot nummer met kleur
   - Level label (ONTSPANNEN, GESTREST, EDGE!, etc)
   - Sensor waarden (HR, Temp, GSR)
   - AI vs Jij vergelijking met correctie

2. AFSPEELBALK MET MARKERS
   - Progress bar met kleur gebaseerd op level
   - AI voorspelling markers (dunne lijntjes)
   - User annotatie markers (dikke groene lijntjes met driehoek)
   - Edge markers (witte driehoekjes)
   - Huidige positie indicator

3. LEVEL GRAFIEK
   - Lijn die stress level over tijd toont
   - Kleurt mee met level

4. SEEK FUNCTIES
   - -10s / +10s knoppen
   - Klik op progress bar om direct te springen

5. SPEED CONTROLS
   - 10% - 200% snelheid
*/
