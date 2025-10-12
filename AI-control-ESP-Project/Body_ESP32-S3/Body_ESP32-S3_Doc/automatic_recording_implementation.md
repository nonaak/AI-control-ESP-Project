# Automatic CSV Session Recording Implementation

## Overview
This document describes the implementation of automatic CSV file recording for all sessions in the Body ESP project. The goal is to ensure that every session (manual, automatic, or ML-driven) creates a .csv file on the SD card for later review and labeling.

## Implementation Summary

### 1. Configuration Option
- **File**: `body_config.h`
- **Setting**: `BODY_CFG.autoRecordSessions = true` (default: enabled)
- This boolean flag controls whether automatic recording is enabled

### 2. Advanced Stress Manager Integration
- **File**: `advanced_stress_manager.cpp`
- **Function**: `startSession()` - Automatically starts CSV recording when ML stress sessions begin
- **Function**: `endSession()` - Automatically stops CSV recording when ML stress sessions end
- **External Functions**: Added declarations for `startRecording()`, `stopRecording()`, and `isRecording`

### 3. AI Test Mode Integration  
- **File**: `Body_ESP.ino`
- **Function**: `startAITest()` - Auto-starts recording for AI Test sessions
- **Function**: `stopAITest()` - Auto-stops recording when AI Test sessions end

### 4. AI Stress Management Integration
- **File**: `Body_ESP.ino` 
- **Function**: `startAIStressManagement()` - Auto-starts recording for AI Stress sessions
- **Function**: `stopAIStressManagement()` - Auto-stops recording when AI Stress sessions end

### 5. ML Training Mode Integration
- **File**: `ml_training_view.cpp`
- **Function**: `mlTraining_begin()` - Auto-starts recording when entering ML Training mode
- **Mode Exit**: Recording is stopped when returning to main screen via `enterMain()`

### 6. Session Cleanup
- **File**: `Body_ESP.ino`
- **Function**: `enterMain()` - Stops auto-recording when returning to main screen (session ends)

### 7. Factory Reset Integration
- **File**: `factory_reset.cpp`
- **Function**: `resetSettings()` - Sets `autoRecordSessions = true` during factory reset

## Recording Behavior

### When Recording Starts Automatically:
1. **AI Stress Management**: When `startAIStressManagement()` is called
2. **AI Test Mode**: When `startAITest()` is called  
3. **ML Training Mode**: When entering ML Training interface
4. **Advanced Stress Sessions**: When `stressManager.startSession()` is called

### When Recording Stops Automatically:
1. **Session End**: When any of the above session types end via their stop functions
2. **Mode Exit**: When returning to main screen from any session mode
3. **Manual Override**: User can still manually start/stop recording via REC button

### Data Logged in CSV Files:
- Time, Heart Rate, Temperature, Skin (GSR), Oxygen, Beat Detection
- Trust Speed, Sleeve Speed, Suction Level, Pause Time
- Breathing (Adem), Vibration (Tril), Suction Active Status, Vacuum (mbar)

## Safety Features

1. **Duplicate Prevention**: Checks `!isRecording` before starting auto-recording
2. **Manual Override Respect**: Manual recording still works independently
3. **Configuration Control**: Can be disabled via `BODY_CFG.autoRecordSessions = false`
4. **SD Card Safety**: Only attempts recording if SD card is available

## Usage

### For Users:
- Recording happens automatically - no action needed
- All sessions are logged to data*.csv files on SD card  
- Manual REC button still works for additional recordings
- Files can be analyzed later using AI Training (12-button feedback) or ML Training interfaces

### For Developers:
- Set `BODY_CFG.autoRecordSessions = false` to disable if needed
- Recording files follow existing format and numbering system
- Integration is non-invasive - existing manual recording unchanged

## Benefits

1. **Complete Data Coverage**: Every session is recorded for ML training
2. **No User Action Required**: Automatic operation reduces missed sessions
3. **Backward Compatible**: Existing manual recording still works
4. **Configurable**: Can be enabled/disabled as needed
5. **ML Training Ready**: All recorded data can be labeled using existing 12-button feedback system

## Testing Checklist

- [ ] AI Stress Management sessions create CSV files
- [ ] AI Test Mode sessions create CSV files  
- [ ] ML Training mode creates CSV files
- [ ] Manual REC button still works independently
- [ ] Auto-recording stops when returning to main screen
- [ ] SD card error handling works correctly
- [ ] Configuration option works (enable/disable)
- [ ] Factory reset sets correct default value

This implementation ensures that all Body ESP sessions generate data for ML training while maintaining full backward compatibility with existing manual recording functionality.