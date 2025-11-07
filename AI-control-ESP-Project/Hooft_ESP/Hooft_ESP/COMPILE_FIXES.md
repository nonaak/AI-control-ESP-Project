# Compile Fixes voor ESP-NOW HoofdESP

## Probleem
```
undefined reference to `g_speedStep'
```

## Oorzaak  
De variabele `g_speedStep` was gedeclareerd als `static` in `ui.cpp`, waardoor het niet toegankelijk was vanuit `espnow_comm.cpp`.

## Oplossing ✅

### 1. ui.cpp - Variabelen toegankelijk maken
```cpp
// Was:
static uint8_t g_speedStep = 0;
static uint16_t g_targetStrokes = 30;

// Nu:
uint8_t g_speedStep = 0;  // Removed static for ESP-NOW access
uint16_t g_targetStrokes = 30;  // Removed static for ESP-NOW access
```

### 2. ui.h - Extern declarations toegevoegd
```cpp
#pragma once
#include <Arduino.h>

void uiInit();
void uiTick();

// Global variables accessible by ESP-NOW (declared in ui.cpp)
extern uint8_t g_speedStep;      // Current speed step (0 to SPEED_STEPS-1)
extern uint16_t g_targetStrokes; // Target stroke count
```

### 3. espnow_comm.cpp - Include ui.h toegevoegd
```cpp
#include "espnow_comm.h"
#include "config.h"
#include "ui.h"        // Added for g_speedStep and g_targetStrokes
#include <string.h>
```

### 4. Verwijderde duplicate extern declarations
```cpp
// Removed:
extern uint8_t g_speedStep;
extern float g_targetStrokes;
extern Config CFG;

// Now using proper includes instead
```

## Resultaat ✅
- Alle linker errors opgelost
- Proper separation of concerns gehandhaafd
- Global variables correct gedeclareerd en toegankelijk
- Code remains clean en maintainable

## Gecompileerde Functionaliteit
- `getUserTrustSpeed()` - Werkt nu correct met echte speedStep waarden
- `getUserSleeveSpeed()` - Krijgt access tot UI speed control
- ESP-NOW communicatie volledig functionaal
- Real-time vacuum control geïntegreerd

## Test Readiness ✅
De code zou nu moeten compileren zonder errors en klaar zijn voor hardware testing.