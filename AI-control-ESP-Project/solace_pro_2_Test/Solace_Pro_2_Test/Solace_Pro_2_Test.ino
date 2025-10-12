/*
 * Solace Pro 2 Test ESP - TTGO T-Display V1.1
 * 
 * Test programma voor Solace Pro 2 verbinding met visuele feedback
 * - Menu met Keon en Solace Pro 2 verbindingsopties
 * - Sleeve beweging visualisatie voor Solace Pro 2
 * - Nunchuk besturing op I2C pinnen 21 en 22
 * - TTGO T-Display 1.14 inch (135x240)
 * 
 * Hardware: LilyGO TTGO T-Display V1.1 ESP32
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <NintendoExtensionCtrl.h>
#include <NimBLEDevice.h>
#include <NimBLEClient.h>
#include <NimBLEScan.h>

// Display setup
TFT_eSPI tft = TFT_eSPI(); 
Nunchuk nchuk;

// Lovense BLE Protocol Constants (reverse engineered from community)
// Primary Lovense service UUID (used by most Lovense devices)
static NimBLEUUID lovenseServiceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static NimBLEUUID lovenseWriteCharUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");   // Write characteristic
static NimBLEUUID lovenseReadCharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");    // Notify characteristic

// Alternative Lovense UUIDs (some devices use different UUIDs)
static NimBLEUUID altLovenseServiceUUID("53300001-0023-4bd4-bbd5-a6920e4c5653");
static NimBLEUUID altLovenseWriteCharUUID("53300002-0023-4bd4-bbd5-a6920e4c5653");
static NimBLEUUID altLovenseReadCharUUID("53300003-0023-4bd4-bbd5-a6920e4c5653");

// BLE objects
NimBLEClient* pSolaceClient = nullptr;
NimBLEClient* pKeonClient = nullptr;  // Keep for compatibility
NimBLERemoteCharacteristic* pSolaceWriteChar = nullptr;
NimBLERemoteCharacteristic* pSolaceReadChar = nullptr;
NimBLEScan* pBLEScan = nullptr;

// BLE device info
struct DeviceInfo {
  NimBLEAddress address;
  std::string name;
  bool isLovense = false;
  bool isKiiroo = false;  // Keep for Keon compatibility
};

std::vector<DeviceInfo> discoveredDevices;

// Display constants
const int SCREEN_W = 135;
const int SCREEN_H = 240;

// TTGO buttons
const int BUTTON_LEFT = 0;   // GPIO0 (BOOT button)
const int BUTTON_RIGHT = 35; // GPIO35

// Colors
const uint16_t COL_BG = TFT_BLACK;
const uint16_t COL_TEXT = TFT_WHITE;
const uint16_t COL_SELECTED = TFT_GREEN;
const uint16_t COL_RED = TFT_RED;
const uint16_t COL_GREEN = TFT_GREEN;
const uint16_t COL_BLUE = TFT_BLUE;
const uint16_t COL_PURPLE = TFT_MAGENTA;  // Solace signature color
const uint16_t COL_CYAN = TFT_CYAN;
const uint16_t COL_GRAY = TFT_DARKGREY;

// App states
enum AppState {
  STATE_MENU,
  STATE_CONNECTION_POPUP,
  STATE_SOLACE_DISPLAY
};

AppState currentState = STATE_MENU;

// Menu variables
int menuIndex = 0;
int popupChoice = 0;
int popupDevice = -1;

// Connection status
bool keonConnected = false;
bool solaceConnected = false;
bool nunchukReady = false;
bool bleInitialized = false;

// BLE timing
uint32_t lastBLECommand = 0;
const uint32_t BLE_COMMAND_INTERVAL = 100; // 10Hz max for Solace Pro 2 (slower than Keon)

// Current Solace Pro 2 state
uint8_t currentSolacePosition = 50;    // 0-100 (50 = middle)
uint8_t currentSolaceSpeed = 0;        // 0-20 (Lovense speed range)
uint8_t currentSolaceOscillation = 0;  // 0-20 oscillation level

// Solace Pro 2 display variables
float sleevePosition = 0.5f;  // 0.0 = top, 1.0 = bottom
float targetPosition = 0.5f;
float strokeIntensity = 0.5f; // 0.0 = gentle, 1.0 = intense
bool animating = false;

// Input handling
const int JOY_THRESHOLD = 50;
uint32_t lastInputTime = 0;
uint32_t lastButtonTime = 0;
const uint32_t INPUT_DELAY = 150;
const uint32_t BUTTON_DEBOUNCE = 100;

// Animation
uint32_t lastAnimTime = 0;
const uint32_t ANIM_INTERVAL = 50; // 20 FPS

// Input state structure
struct InputState {
  int jx, jy;
  bool c, z;
  bool leftButton, rightButton;
  bool leftPressed, rightPressed;
};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Solace Pro 2 Test ESP Starting ===");
  
  // Initialize buttons
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  
  // Initialize display
  tft.init();
  tft.setRotation(0); // Portrait
  tft.fillScreen(COL_BG);
  
  // Startup screen
  showStartupScreen();
  
  // Initialize BLE
  Serial.println("[BLE] Initializing NimBLE for Solace Pro 2...");
  NimBLEDevice::init("Solace_Pro_2_Test_ESP");
  pBLEScan = NimBLEDevice::getScan();
  bleInitialized = true;
  Serial.println("[BLE] NimBLE initialized successfully");
  
  // Initialize I2C for nunchuk
  Wire.begin(21, 22);
  Serial.println("[I2C] Initialized on SDA=21, SCL=22");
  
  // Try nunchuk connection
  delay(500);
  if (nchuk.connect()) {
    nunchukReady = true;
    Serial.println("[NUNCHUK] Connected successfully");
  } else {
    Serial.println("[NUNCHUK] Failed to connect - using buttons only");
  }
  
  delay(1500);
  drawMenu();
  
  Serial.println("[SETUP] Solace Pro 2 Test ready!");
  Serial.println("\n*** Type 'help' in Serial Monitor for all available commands! ***");
}

void loop() {
  // Handle serial commands
  handleSerialCommands();
  
  // Maintain nunchuk connection
  updateNunchuk();
  
  // Get input
  InputState input = getInput();
  
  // Handle state machine
  switch (currentState) {
    case STATE_MENU:
      handleMenu(input);
      break;
    case STATE_CONNECTION_POPUP:
      handlePopup(input);
      break;
    case STATE_SOLACE_DISPLAY:
      handleSolaceDisplay(input);
      break;
  }
  
  // Animation update
  if (millis() - lastAnimTime > ANIM_INTERVAL) {
    updateAnimation();
    lastAnimTime = millis();
  }
  
  delay(10);
}

InputState getInput() {
  static bool leftPrev = true, rightPrev = true;
  
  InputState input = {128, 128, false, false, false, false, false, false};
  
  // Nunchuk input
  if (nunchukReady) {
    input.jx = nchuk.joyX();
    input.jy = nchuk.joyY();
    input.c = nchuk.buttonC();
    input.z = nchuk.buttonZ();
  }
  
  // Button input
  bool leftNow = digitalRead(BUTTON_LEFT) == LOW;
  bool rightNow = digitalRead(BUTTON_RIGHT) == LOW;
  
  input.leftButton = leftNow;
  input.rightButton = rightNow;
  input.leftPressed = leftNow && !leftPrev;
  input.rightPressed = rightNow && !rightPrev;
  
  leftPrev = leftNow;
  rightPrev = rightNow;
  
  return input;
}

void updateNunchuk() {
  static uint32_t lastTry = 0;
  
  if (nunchukReady) {
    if (!nchuk.update()) {
      nunchukReady = false;
      Serial.println("[NUNCHUK] Connection lost");
    }
  } else if (millis() - lastTry > 2000) {
    lastTry = millis();
    if (nchuk.connect()) {
      nunchukReady = true;
      Serial.println("[NUNCHUK] Reconnected");
    }
  }
}

void handleMenu(InputState input) {
  uint32_t now = millis();
  static bool zPrev = false;
  
  // Navigation
  if (now - lastInputTime > INPUT_DELAY) {
    bool moved = false;
    
    // Joystick navigation
    if (input.jy > 128 + JOY_THRESHOLD && menuIndex > 0) {
      menuIndex--;
      moved = true;
    } else if (input.jy < 128 - JOY_THRESHOLD && menuIndex < 1) {
      menuIndex++;
      moved = true;
    }
    
    if (moved) {
      lastInputTime = now;
      drawMenu();
      Serial.printf("[MENU] Selected: %s\n", menuIndex == 0 ? "Keon" : "Solace Pro 2");
    }
  }
  
  // Button navigation
  if (now - lastButtonTime > BUTTON_DEBOUNCE) {
    if (input.leftPressed) {
      menuIndex = (menuIndex == 0) ? 1 : 0;
      lastButtonTime = now;
      drawMenu();
      Serial.printf("[MENU] Button nav: %s\n", menuIndex == 0 ? "Keon" : "Solace Pro 2");
    }
    
    if (input.rightPressed) {
      openConnectionPopup();
      lastButtonTime = now;
    }
  }
  
  // Z button
  if (input.z && !zPrev) {
    openConnectionPopup();
  }
  
  zPrev = input.z;
}

void handlePopup(InputState input) {
  uint32_t now = millis();
  static bool zPrev = false, cPrev = false;
  
  // Navigation
  if (now - lastInputTime > INPUT_DELAY) {
    bool moved = false;
    
    if (input.jx < 128 - JOY_THRESHOLD && popupChoice == 1) {
      popupChoice = 0;
      moved = true;
    } else if (input.jx > 128 + JOY_THRESHOLD && popupChoice == 0) {
      popupChoice = 1;
      moved = true;
    }
    
    if (moved) {
      lastInputTime = now;
      drawPopup();
      Serial.printf("[POPUP] Choice: %s\n", popupChoice == 0 ? "Ja" : "Nee");
    }
  }
  
  // Button navigation
  if (now - lastButtonTime > BUTTON_DEBOUNCE) {
    if (input.leftPressed) {
      popupChoice = (popupChoice == 0) ? 1 : 0;
      lastButtonTime = now;
      drawPopup();
    }
    
    if (input.rightPressed) {
      if (popupChoice == 0) {
        executeConnection();
      }
      closePopup();
      lastButtonTime = now;
    }
  }
  
  // Z button - confirm
  if (input.z && !zPrev) {
    if (popupChoice == 0) {
      executeConnection();
    }
    closePopup();
  }
  
  // C button - cancel
  if (input.c && !cPrev) {
    closePopup();
  }
  
  zPrev = input.z;
  cPrev = input.c;
}

void handleSolaceDisplay(InputState input) {
  static bool cPrev = false;
  
  // Update sleeve position based on nunchuk Y
  if (nunchukReady) {
    // Map joystick Y (0-255) to position (0.0-1.0)
    targetPosition = 1.0f - ((float)input.jy / 255.0f);
    targetPosition = constrain(targetPosition, 0.0f, 1.0f);
    
    // Map joystick X (0-255) to stroke intensity (0.0-1.0)
    strokeIntensity = (float)input.jx / 255.0f;
    strokeIntensity = constrain(strokeIntensity, 0.0f, 1.0f);
    
    // Convert to Solace Pro 2 ranges and send commands
    uint8_t newPosition = (uint8_t)(targetPosition * 100.0f);  // 0-100 for Solace
    uint8_t newSpeed = 0;
    
    // Use stroke intensity for oscillation/thrusting speed (0-20 Lovense range)
    if (strokeIntensity > 0.1f) { // Deadzone
      newSpeed = (uint8_t)(strokeIntensity * 20.0f);
      newSpeed = constrain(newSpeed, 1, 20);
    }
    
    // Send command to Solace Pro 2 if connected and values changed
    if (solaceConnected && pSolaceWriteChar && 
        (newPosition != currentSolacePosition || newSpeed != currentSolaceSpeed) &&
        (millis() - lastBLECommand >= BLE_COMMAND_INTERVAL)) {
      
      // Send position command
      if (newPosition != currentSolacePosition) {
        if (sendSolacePositionCommand(newPosition)) {
          currentSolacePosition = newPosition;
        }
      }
      
      // Send thrusting/oscillation command
      if (newSpeed != currentSolaceSpeed) {
        if (sendSolaceOscillationCommand(newSpeed)) {
          currentSolaceSpeed = newSpeed;
        }
      }
      
      lastBLECommand = millis();
    }
  }
  
  // C button - return to menu
  if (input.c && !cPrev) {
    // Stop Solace movement before returning to menu
    if (solaceConnected && pSolaceWriteChar) {
      sendSolaceOscillationCommand(0); // Stop oscillation
      currentSolaceSpeed = 0;
    }
    currentState = STATE_MENU;
    drawMenu();
    Serial.println("[SOLACE] Returned to menu");
  }
  
  cPrev = input.c;
}

void executeConnection() {
  if (popupDevice == 0) { // Keon
    if (keonConnected) {
      // Disconnect Keon (placeholder for now)
      keonConnected = false;
      Serial.println("[CONNECTION] Keon disconnected");
    } else {
      // Try to connect to Keon (placeholder - not implemented in Solace project)
      Serial.println("[CONNECTION] Keon connection not implemented in Solace Pro 2 project");
    }
  } else { // Solace Pro 2
    if (solaceConnected) {
      // Disconnect Solace Pro 2
      disconnectLovenseDevice(&pSolaceClient);
      pSolaceWriteChar = nullptr;
      pSolaceReadChar = nullptr;
      solaceConnected = false;
      Serial.println("[CONNECTION] Solace Pro 2 disconnected");
    } else {
      // Try to connect to Solace Pro 2
      Serial.println("[CONNECTION] Attempting to connect to Solace Pro 2...");
      bool success = attemptSolaceConnection();
      if (success) {
        solaceConnected = true;
        showConnectionAnimation("Solace Pro 2");
        // Switch to Solace display
        currentState = STATE_SOLACE_DISPLAY;
        drawSolaceDisplay();
        Serial.println("[SOLACE] Connected successfully - switched to display mode");
      } else {
        Serial.println("[SOLACE] Connection failed - device not found");
      }
    }
  }
}

bool attemptSolaceConnection() {
  Serial.println("[SOLACE] Starting Solace Pro 2 connection attempt...");
  
  if (!bleInitialized) {
    Serial.println("[SOLACE] BLE not initialized!");
    return false;
  }
  
  // Scan for Lovense devices
  if (!scanForLovenseDevices(8)) {
    Serial.println("[SOLACE] No Lovense devices found during scan");
    return false;
  }
  
  // Look for a Solace Pro 2 specifically (or any Lovense device for testing)
  DeviceInfo* targetDevice = nullptr;
  for (auto& device : discoveredDevices) {
    if (device.isLovense) {
      // Prefer devices with "Solace" in name
      if (device.name.find("Solace") != std::string::npos) {
        targetDevice = &device;
        break;
      } else if (device.name.find("LVS-") == 0 && !targetDevice) {
        // Fallback to first LVS- device found
        targetDevice = &device;
      }
    }
  }
  
  if (!targetDevice) {
    Serial.println("[SOLACE] No suitable Lovense device found");
    return false;
  }
  
  Serial.printf("[SOLACE] Attempting to connect to: %s\n", targetDevice->name.c_str());
  
  // Attempt connection
  bool connected = connectToLovenseDevice(targetDevice->address, &pSolaceClient,
                                         &pSolaceWriteChar, &pSolaceReadChar);
  
  if (connected) {
    Serial.println("[SOLACE] Successfully connected and initialized!");
    currentSolacePosition = 50; // Reset to middle position
    currentSolaceSpeed = 0;     // Reset speed
    return true;
  } else {
    Serial.println("[SOLACE] Failed to connect to device");
    return false;
  }
}

void openConnectionPopup() {
  popupDevice = menuIndex;
  popupChoice = 0;
  currentState = STATE_CONNECTION_POPUP;
  drawPopup();
  Serial.printf("[POPUP] Opened for %s\n", menuIndex == 0 ? "Keon" : "Solace Pro 2");
}

void closePopup() {
  currentState = STATE_MENU;
  popupDevice = -1;
  popupChoice = 0;
  drawMenu();
  Serial.println("[POPUP] Closed");
}

void updateAnimation() {
  if (currentState == STATE_SOLACE_DISPLAY) {
    // Smooth animation towards target
    float diff = targetPosition - sleevePosition;
    if (abs(diff) > 0.01f) {
      sleevePosition += diff * 0.15f; // Slightly faster than Keon for Pro 2
      drawSolaceDisplay();
    }
  }
}

void showStartupScreen() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_PURPLE);
  tft.setTextSize(2);
  tft.setCursor(5, 50);
  tft.println("SOLACE");
  tft.setCursor(15, 75);
  tft.println("PRO 2");
  
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.println("TTGO T-Display V1.1");
  tft.setCursor(10, 130);
  tft.println("Initializing...");
  
  Serial.println("[DISPLAY] Startup screen shown");
}

void showConnectionAnimation(const char* device) {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_PURPLE);
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.println("Connecting");
  
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.println(device);
  
  // Animation dots with purple theme
  for (int i = 0; i < 3; i++) {
    tft.setTextColor(COL_PURPLE);
    tft.setTextSize(2);
    tft.setCursor(10 + i * 15, 140);
    tft.println(".");
    delay(300);
  }
  
  tft.setTextColor(COL_GREEN);
  tft.setCursor(10, 170);
  tft.println("Connected!");
  delay(1000);
}

void drawMenu() {
  tft.fillScreen(COL_BG);
  
  // Title with Solace branding
  tft.setTextColor(COL_PURPLE);
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.println("SOLACE");
  tft.setCursor(15, 25);
  tft.println("TEST");
  
  // Menu items
  for (int i = 0; i < 2; i++) {
    int y = 55 + i * 40;
    bool selected = (i == menuIndex);
    bool connected = (i == 0) ? keonConnected : solaceConnected;
    
    // Selection highlight
    if (selected) {
      tft.fillRect(5, y - 5, 125, 35, COL_GRAY);
    }
    
    // Text
    tft.setTextColor(selected ? COL_SELECTED : COL_TEXT);
    tft.setTextSize(1);
    tft.setCursor(10, y);
    if (i == 0) {
      tft.println("Keon");
    } else {
      tft.println("Solace Pro 2");
    }
    
    // Status dot
    uint16_t dotColor = connected ? COL_GREEN : COL_RED;
    tft.fillCircle(110, y + 4, 6, dotColor);
    tft.drawCircle(110, y + 4, 6, COL_TEXT);
  }
  
  // Status info
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(10, 145);
  tft.printf("Keon: %s", keonConnected ? "CONNECTED" : "OFF");
  tft.setCursor(10, 160);
  tft.printf("Solace: %s", solaceConnected ? "CONNECTED" : "OFF");
  tft.setCursor(10, 175);
  tft.printf("Nunchuk: %s", nunchukReady ? "OK" : "FAIL");
  
  // Help text
  tft.setTextColor(COL_GRAY);
  tft.setCursor(5, 200);
  tft.println("JY/LEFT: navigate");
  tft.setCursor(5, 215);
  tft.println("Z/RIGHT: select");
}

void drawPopup() {
  // Popup overlay with purple theme
  int px = 15, py = 70, pw = 105, ph = 100;
  
  tft.fillRoundRect(px, py, pw, ph, 8, COL_PURPLE);
  tft.drawRoundRect(px, py, pw, ph, 8, COL_TEXT);
  
  // Title
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(px + 5, py + 10);
  
  const char* device = (popupDevice == 0) ? "Keon" : "Solace Pro 2";
  bool connected = (popupDevice == 0) ? keonConnected : solaceConnected;
  
  if (connected) {
    tft.println("Disconnect");
  } else {
    tft.println("Connect to");
  }
  
  tft.setTextSize(1);
  tft.setCursor(px + 5, px + 25);
  tft.println(device);
  tft.setCursor(px + 70, py + 35);
  tft.println("?");
  
  // Choices
  for (int i = 0; i < 2; i++) {
    int cx = px + 15 + i * 40;
    int cy = py + 55;
    bool selected = (i == popupChoice);
    
    if (selected) {
      tft.fillRect(cx - 5, cy - 5, 25, 20, COL_GRAY);
    }
    
    tft.setTextColor(selected ? COL_GREEN : COL_TEXT);
    tft.setTextSize(2);
    tft.setCursor(cx, cy);
    tft.println(i == 0 ? "Ja" : "Nee");
  }
  
  // Help
  tft.setTextColor(COL_GRAY);
  tft.setTextSize(1);
  tft.setCursor(px + 5, py + ph - 15);
  tft.println("JX:choose Z:OK C:cancel");
}

void drawSolaceDisplay() {
  tft.fillScreen(COL_BG);
  
  // Title with Pro 2 branding
  tft.setTextColor(COL_PURPLE);
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.println("SOLACE");
  tft.setCursor(10, 25);
  tft.println("PRO 2");
  
  // Sleeve track (vertical line) - slightly different from Keon
  int trackX = 65;
  int trackTop = 55;
  int trackHeight = 130;
  int trackBottom = trackTop + trackHeight;
  
  // Dual track lines for Pro 2
  tft.drawLine(trackX - 1, trackTop, trackX - 1, trackBottom, COL_PURPLE);
  tft.drawLine(trackX + 1, trackTop, trackX + 1, trackBottom, COL_PURPLE);
  
  // Sleeve rectangle with Pro 2 styling
  int sleeveHeight = 25;
  int sleeveWidth = 18;
  int sleeveY = trackTop + (int)(sleevePosition * (trackHeight - sleeveHeight));
  int sleeveX = trackX - sleeveWidth / 2;
  
  // Clear previous position
  tft.fillRect(sleeveX - 3, trackTop - 2, sleeveWidth + 6, trackHeight + 4, COL_BG);
  
  // Redraw tracks
  tft.drawLine(trackX - 1, trackTop, trackX - 1, trackBottom, COL_PURPLE);
  tft.drawLine(trackX + 1, trackTop, trackX + 1, trackBottom, COL_PURPLE);
  
  // Draw sleeve with intensity-based color
  uint16_t sleeveColor = COL_CYAN;
  if (strokeIntensity > 0.7f) {
    sleeveColor = COL_PURPLE;
  } else if (strokeIntensity > 0.4f) {
    sleeveColor = COL_BLUE;
  }
  
  tft.fillRect(sleeveX, sleeveY, sleeveWidth, sleeveHeight, sleeveColor);
  tft.drawRect(sleeveX, sleeveY, sleeveWidth, sleeveHeight, COL_TEXT);
  
  // Intensity bar
  int barX = 95;
  int barY = trackTop;
  int barH = trackHeight;
  int barW = 8;
  
  tft.drawRect(barX, barY, barW, barH, COL_TEXT);
  int fillH = (int)(strokeIntensity * barH);
  tft.fillRect(barX + 1, barY + barH - fillH, barW - 2, fillH, COL_PURPLE);
  
  // Position and intensity info
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(5, 195);
  tft.printf("Position: %.2f", sleevePosition);
  tft.setCursor(5, 210);
  tft.printf("Intensity: %.2f", strokeIntensity);
  
  // Instructions
  tft.setTextColor(COL_GRAY);
  tft.setCursor(5, SCREEN_H - 15);
  tft.println("JY:pos JX:power C:menu");
}

// ====== LOVENSE BLE FUNCTIONS ======

class LovenseScanCallback : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    std::string deviceName = advertisedDevice->getName();
    
    // Check if this looks like a Lovense device
    bool isLovense = false;
    if (deviceName.find("LVS-") == 0 ||  // Lovense devices often start with LVS-
        deviceName.find("Solace") != std::string::npos ||
        deviceName.find("Max") != std::string::npos ||
        deviceName.find("Nora") != std::string::npos ||
        deviceName.find("Edge") != std::string::npos ||
        deviceName.find("Hush") != std::string::npos ||
        deviceName.find("Lush") != std::string::npos) {
      isLovense = true;
    }
    
    // Check for Lovense service UUIDs
    if (advertisedDevice->isAdvertisingService(lovenseServiceUUID) ||
        advertisedDevice->isAdvertisingService(altLovenseServiceUUID)) {
      isLovense = true;
    }
    
    // Also check for Keon compatibility (keep existing Keon support)
    bool isKiiroo = false;
    if (deviceName.find("Launch") != std::string::npos ||
        deviceName.find("Keon") != std::string::npos ||
        deviceName.find("Onyx") != std::string::npos) {
      isKiiroo = true;
    }
    
    if (isLovense || isKiiroo || deviceName.length() > 0) {
      DeviceInfo device;
      device.address = advertisedDevice->getAddress();
      device.name = deviceName.length() > 0 ? deviceName : "Unknown Device";
      device.isLovense = isLovense;
      device.isKiiroo = isKiiroo;
      
      // Check if we already have this device
      bool found = false;
      for (auto& existing : discoveredDevices) {
        if (existing.address == device.address) {
          found = true;
          break;
        }
      }
      
      if (!found) {
        discoveredDevices.push_back(device);
        const char* deviceType = isLovense ? "Lovense " : (isKiiroo ? "Kiiroo " : "");
        Serial.printf("[BLE] Found %sdevice: %s (%s)\n", 
                     deviceType,
                     device.name.c_str(), 
                     device.address.toString().c_str());
      }
    }
  }
};

bool scanForLovenseDevices(uint32_t scanTime = 10) {
  if (!bleInitialized) {
    Serial.println("[BLE] BLE not initialized!");
    return false;
  }
  
  Serial.printf("[BLE] Scanning for Lovense devices (%d seconds)...\n", scanTime);
  discoveredDevices.clear();
  
  pBLEScan->setAdvertisedDeviceCallbacks(new LovenseScanCallback());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  NimBLEScanResults results = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();
  
  Serial.printf("[BLE] Scan completed. Found %d potential devices\n", discoveredDevices.size());
  
  // List all found Lovense devices
  int lovenseCount = 0;
  for (auto& device : discoveredDevices) {
    if (device.isLovense) {
      lovenseCount++;
      Serial.printf("[BLE] Lovense device #%d: %s (%s)\n", 
                   lovenseCount, device.name.c_str(), device.address.toString().c_str());
    }
  }
  
  return lovenseCount > 0;
}

bool connectToLovenseDevice(NimBLEAddress deviceAddress, NimBLEClient** client, 
                           NimBLERemoteCharacteristic** writeChar,
                           NimBLERemoteCharacteristic** readChar) {
  
  Serial.printf("[BLE] Connecting to Lovense device %s...\n", deviceAddress.toString().c_str());
  
  // Create client
  *client = NimBLEDevice::createClient();
  if (!*client) {
    Serial.println("[BLE] Failed to create client");
    return false;
  }
  
  // Connect to device
  if (!(*client)->connect(deviceAddress)) {
    Serial.println("[BLE] Failed to connect to device");
    NimBLEDevice::deleteClient(*client);
    *client = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Connected! Discovering Lovense services...");
  
  // Try primary Lovense service first
  NimBLERemoteService* pService = (*client)->getService(lovenseServiceUUID);
  if (!pService) {
    Serial.println("[BLE] Primary Lovense service not found, trying alternative...");
    pService = (*client)->getService(altLovenseServiceUUID);
  }
  
  if (!pService) {
    Serial.println("[BLE] No Lovense service found!");
    (*client)->disconnect();
    NimBLEDevice::deleteClient(*client);
    *client = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Lovense service found! Getting characteristics...");
  
  // Get characteristics (try both UUID sets)
  *writeChar = pService->getCharacteristic(lovenseWriteCharUUID);
  if (!*writeChar) {
    *writeChar = pService->getCharacteristic(altLovenseWriteCharUUID);
  }
  
  *readChar = pService->getCharacteristic(lovenseReadCharUUID);
  if (!*readChar) {
    *readChar = pService->getCharacteristic(altLovenseReadCharUUID);
  }
  
  // Check if we have the essential write characteristic
  if (!*writeChar) {
    Serial.println("[BLE] Write characteristic not found!");
    (*client)->disconnect();
    NimBLEDevice::deleteClient(*client);
    *client = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Essential characteristics found!");
  
  // Enable notifications on read characteristic if available
  if (*readChar && (*readChar)->canNotify()) {
    Serial.println("[BLE] Enabling status notifications...");
    (*readChar)->subscribe(true);
  }
  
  Serial.println("[BLE] Lovense device connected successfully!");
  return true;
}

bool sendSolacePositionCommand(uint8_t position) {
  if (!pSolaceWriteChar || !pSolaceWriteChar->canWrite()) {
    return false;
  }
  
  // Lovense position command format: "Preset:POSITION;" (position 0-100)
  String command = "Preset:" + String(position) + ";";
  
  bool success = pSolaceWriteChar->writeValue(command.c_str(), command.length(), false);
  
  if (success) {
    Serial.printf("[SOLACE] Position command sent: %d\n", position);
  } else {
    Serial.println("[SOLACE] Failed to send position command!");
  }
  
  return success;
}

bool sendSolaceOscillationCommand(uint8_t speed) {
  if (!pSolaceWriteChar || !pSolaceWriteChar->canWrite()) {
    return false;
  }
  
  // Lovense oscillation/thrusting command: "Oscillate:SPEED;" (speed 0-20)
  String command = "Oscillate:" + String(speed) + ";";
  
  bool success = pSolaceWriteChar->writeValue(command.c_str(), command.length(), false);
  
  if (success) {
    Serial.printf("[SOLACE] Oscillation command sent: %d\n", speed);
  } else {
    Serial.println("[SOLACE] Failed to send oscillation command!");
  }
  
  return success;
}

bool sendSolaceFunctionCommand(uint8_t strokeMin, uint8_t strokeMax, uint8_t thrusting) {
  if (!pSolaceWriteChar || !pSolaceWriteChar->canWrite()) {
    return false;
  }
  
  // Lovense function command: "Function:STROKEMIN-STROKEMAX,Thrusting:LEVEL;"
  String command = "Function:" + String(strokeMin) + "-" + String(strokeMax) + ",Thrusting:" + String(thrusting) + ";";
  
  bool success = pSolaceWriteChar->writeValue(command.c_str(), command.length(), false);
  
  if (success) {
    Serial.printf("[SOLACE] Function command sent: stroke %d-%d, thrusting %d\n", strokeMin, strokeMax, thrusting);
  } else {
    Serial.println("[SOLACE] Failed to send function command!");
  }
  
  return success;
}

void disconnectLovenseDevice(NimBLEClient** client) {
  if (*client) {
    if ((*client)->isConnected()) {
      (*client)->disconnect();
    }
    NimBLEDevice::deleteClient(*client);
    *client = nullptr;
  }
}

// ====== SERIAL COMMAND INTERFACE ======

void printHelp() {
  Serial.println("\n=== SOLACE PRO 2 TEST - SERIAL COMMANDS ===");
  Serial.println("BLE Commands:");
  Serial.println("  scan              - Scan for Lovense devices");
  Serial.println("  connect <addr>    - Connect to device by address (e.g., connect aa:bb:cc:dd:ee:ff)");
  Serial.println("  solace            - Try to connect to Solace Pro 2 (same as menu)");
  Serial.println("  keon              - Try to connect to Keon (placeholder)");
  Serial.println("  disconnect        - Disconnect current device");
  Serial.println("  status            - Show connection status");
  Serial.println();
  Serial.println("Solace Pro 2 Control Commands (when connected):");
  Serial.println("  position <0-100>  - Set position (0=one end, 100=other end)");
  Serial.println("  oscillate <0-20>  - Set oscillation/thrusting speed (0=stop, 20=max)");
  Serial.println("  stroke <min> <max> <thrust> - Set stroke range and thrusting level");
  Serial.println("  preset <0-100>    - Move to preset position");
  Serial.println("  stop              - Stop all movement");
  Serial.println("  demo              - Run automated demo sequence");
  Serial.println();
  Serial.println("Test Commands:");
  Serial.println("  test stroke       - Test stroke patterns (gentle-medium-intense)");
  Serial.println("  test positions    - Test all positions 0-100");
  Serial.println("  test oscillate    - Test oscillation speeds 0-20");
  Serial.println("  test wave         - Smooth wave motion with varying intensity");
  Serial.println("  test patterns     - Test various pattern combinations");
  Serial.println();
  Serial.println("Info Commands:");
  Serial.println("  devices           - List discovered devices");
  Serial.println("  nunchuk           - Show nunchuk status");
  Serial.println("  help              - Show this help");
  Serial.println("========================================");
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "help" || command == "?") {
      printHelp();
    }
    
    else if (command == "scan") {
      Serial.println("[CMD] Starting BLE scan for Lovense devices...");
      scanForLovenseDevices(10);
      Serial.printf("[CMD] Scan complete. Found %d devices\n", discoveredDevices.size());
    }
    
    else if (command.startsWith("connect ")) {
      String addrStr = command.substring(8);
      addrStr.trim();
      Serial.printf("[CMD] Attempting to connect to %s...\n", addrStr.c_str());
      
      NimBLEAddress addr(addrStr.c_str());
      bool connected = connectToLovenseDevice(addr, &pSolaceClient, 
                                             &pSolaceWriteChar, &pSolaceReadChar);
      if (connected) {
        solaceConnected = true;
        Serial.println("[CMD] Connected successfully!");
      } else {
        Serial.println("[CMD] Connection failed!");
      }
    }
    
    else if (command == "solace") {
      Serial.println("[CMD] Attempting Solace Pro 2 connection...");
      if (attemptSolaceConnection()) {
        solaceConnected = true;
        Serial.println("[CMD] Solace Pro 2 connected!");
      } else {
        Serial.println("[CMD] Solace Pro 2 connection failed!");
      }
    }
    
    else if (command == "keon") {
      Serial.println("[CMD] Keon connection not implemented in Solace Pro 2 project");
    }
    
    else if (command == "disconnect") {
      Serial.println("[CMD] Disconnecting devices...");
      if (solaceConnected) {
        disconnectLovenseDevice(&pSolaceClient);
        pSolaceWriteChar = nullptr;
        pSolaceReadChar = nullptr;
        solaceConnected = false;
        Serial.println("[CMD] Solace Pro 2 disconnected");
      }
      if (keonConnected) {
        keonConnected = false;
        Serial.println("[CMD] Keon disconnected (placeholder)");
      }
    }
    
    else if (command == "status") {
      Serial.println("\n=== STATUS ===");
      Serial.printf("BLE Initialized: %s\n", bleInitialized ? "YES" : "NO");
      Serial.printf("Solace Pro 2 Connected: %s\n", solaceConnected ? "YES" : "NO");
      Serial.printf("Keon Connected: %s\n", keonConnected ? "YES" : "NO");
      Serial.printf("Nunchuk Ready: %s\n", nunchukReady ? "YES" : "NO");
      if (solaceConnected) {
        Serial.printf("Current Position: %d\n", currentSolacePosition);
        Serial.printf("Current Oscillation: %d\n", currentSolaceSpeed);
      }
      Serial.println("============");
    }
    
    else if (command.startsWith("position ")) {
      if (!solaceConnected || !pSolaceWriteChar) {
        Serial.println("[CMD] Solace Pro 2 not connected!");
        return;
      }
      
      int pos = command.substring(9).toInt();
      pos = constrain(pos, 0, 100);
      Serial.printf("[CMD] Setting position to %d\n", pos);
      sendSolacePositionCommand(pos);
      currentSolacePosition = pos;
    }
    
    else if (command.startsWith("oscillate ")) {
      if (!solaceConnected || !pSolaceWriteChar) {
        Serial.println("[CMD] Solace Pro 2 not connected!");
        return;
      }
      
      int speed = command.substring(10).toInt();
      speed = constrain(speed, 0, 20);
      Serial.printf("[CMD] Setting oscillation to %d\n", speed);
      sendSolaceOscillationCommand(speed);
      currentSolaceSpeed = speed;
    }
    
    else if (command.startsWith("stroke ")) {
      if (!solaceConnected || !pSolaceWriteChar) {
        Serial.println("[CMD] Solace Pro 2 not connected!");
        return;
      }
      
      int min, max, thrust;
      if (sscanf(command.c_str(), "stroke %d %d %d", &min, &max, &thrust) == 3) {
        min = constrain(min, 0, 100);
        max = constrain(max, 0, 100);
        thrust = constrain(thrust, 0, 20);
        if (max - min < 20) {
          Serial.println("[CMD] Warning: stroke range should be at least 20 apart");
        }
        Serial.printf("[CMD] Setting stroke range %d-%d with thrusting %d\n", min, max, thrust);
        sendSolaceFunctionCommand(min, max, thrust);
      } else {
        Serial.println("[CMD] Usage: stroke <min 0-100> <max 0-100> <thrust 0-20>");
      }
    }
    
    else if (command.startsWith("preset ")) {
      if (!solaceConnected || !pSolaceWriteChar) {
        Serial.println("[CMD] Solace Pro 2 not connected!");
        return;
      }
      
      int pos = command.substring(7).toInt();
      pos = constrain(pos, 0, 100);
      Serial.printf("[CMD] Moving to preset position %d\n", pos);
      sendSolacePositionCommand(pos);
      currentSolacePosition = pos;
    }
    
    else if (command == "stop") {
      if (!solaceConnected || !pSolaceWriteChar) {
        Serial.println("[CMD] Solace Pro 2 not connected!");
        return;
      }
      Serial.println("[CMD] Stopping all movement");
      sendSolaceOscillationCommand(0);
      currentSolaceSpeed = 0;
    }
    
    else if (command == "demo") {
      runSolaceDemo();
    }
    
    else if (command == "test stroke") {
      testSolaceStrokePattern();
    }
    
    else if (command == "test positions") {
      testSolaceAllPositions();
    }
    
    else if (command == "test oscillate") {
      testSolaceOscillation();
    }
    
    else if (command == "test wave") {
      testSolaceWaveMotion();
    }
    
    else if (command == "test patterns") {
      testSolacePatterns();
    }
    
    else if (command == "devices") {
      Serial.printf("\n=== DISCOVERED DEVICES (%d) ===\n", discoveredDevices.size());
      for (int i = 0; i < discoveredDevices.size(); i++) {
        auto& device = discoveredDevices[i];
        const char* deviceType = device.isLovense ? "Lovense Device" : 
                                (device.isKiiroo ? "Kiiroo Device" : "Other Device");
        Serial.printf("%d. %s (%s) - %s\n", i+1, 
                     device.name.c_str(), 
                     device.address.toString().c_str(),
                     deviceType);
      }
      Serial.println("====================\n");
    }
    
    else if (command == "nunchuk") {
      if (nunchukReady) {
        Serial.printf("Nunchuk: X=%d, Y=%d, C=%d, Z=%d\n", 
                     nchuk.joyX(), nchuk.joyY(), nchuk.buttonC(), nchuk.buttonZ());
      } else {
        Serial.println("Nunchuk not connected");
      }
    }
    
    else if (command.length() > 0) {
      Serial.printf("[CMD] Unknown command: '%s'. Type 'help' for commands.\n", command.c_str());
    }
  }
}

// ====== SOLACE PRO 2 TEST FUNCTIONS ======

void runSolaceDemo() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[DEMO] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[DEMO] Starting Solace Pro 2 demo sequence...");
  
  // Basic position test
  Serial.println("[DEMO] Basic position movements...");
  sendSolacePositionCommand(0);   // One end
  delay(2000);
  sendSolacePositionCommand(100); // Other end
  delay(2000);
  sendSolacePositionCommand(50);  // Middle
  delay(1000);
  
  // Oscillation speed test
  Serial.println("[DEMO] Oscillation speed test...");
  for (int speed = 5; speed <= 15; speed += 5) {
    Serial.printf("[DEMO] Oscillation speed: %d\n", speed);
    sendSolaceOscillationCommand(speed);
    delay(3000);
  }
  
  // Function command test (stroke range + thrusting)
  Serial.println("[DEMO] Stroke function test...");
  sendSolaceFunctionCommand(10, 90, 8);  // Wide stroke, medium thrusting
  delay(4000);
  
  // Stop all
  sendSolaceOscillationCommand(0);
  sendSolacePositionCommand(50);
  Serial.println("[DEMO] Demo completed!");
}

void testSolaceStrokePattern() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[TEST] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[TEST] Testing stroke patterns...");
  
  // Gentle stroke
  Serial.println("[TEST] Gentle stroke pattern...");
  sendSolaceFunctionCommand(20, 80, 3);
  delay(5000);
  
  // Medium stroke
  Serial.println("[TEST] Medium stroke pattern...");
  sendSolaceFunctionCommand(10, 90, 8);
  delay(5000);
  
  // Intense stroke
  Serial.println("[TEST] Intense stroke pattern...");
  sendSolaceFunctionCommand(5, 95, 15);
  delay(5000);
  
  // Stop
  sendSolaceOscillationCommand(0);
  sendSolacePositionCommand(50);
  Serial.println("[TEST] Stroke pattern test completed!");
}

void testSolaceAllPositions() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[TEST] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[TEST] Testing all positions 0-100...");
  
  // Test positions in steps of 10
  for (int pos = 0; pos <= 100; pos += 10) {
    Serial.printf("[TEST] Position: %d\n", pos);
    sendSolacePositionCommand(pos);
    delay(1500);
  }
  
  // Return to middle
  sendSolacePositionCommand(50);
  Serial.println("[TEST] Position test completed!");
}

void testSolaceOscillation() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[TEST] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[TEST] Testing oscillation speeds 0-20...");
  
  // Test oscillation speeds
  for (int speed = 0; speed <= 20; speed += 4) {
    Serial.printf("[TEST] Oscillation speed: %d\n", speed);
    sendSolaceOscillationCommand(speed);
    delay(2000);
  }
  
  // Stop
  sendSolaceOscillationCommand(0);
  Serial.println("[TEST] Oscillation test completed!");
}

void testSolaceWaveMotion() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[TEST] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[TEST] Testing wave motion...");
  
  // Smooth sine wave motion with varying oscillation
  for (int i = 0; i < 360 * 2; i += 15) { // 2 full cycles
    float angle = i * PI / 180.0;
    int pos = 50 + (int)(40 * sin(angle));  // Position 10-90
    int oscillation = 8 + (int)(6 * sin(angle * 2)); // Variable oscillation 2-14
    
    pos = constrain(pos, 0, 100);
    oscillation = constrain(oscillation, 2, 14);
    
    sendSolacePositionCommand(pos);
    delay(50);
    sendSolaceOscillationCommand(oscillation);
    delay(200);
  }
  
  // Stop
  sendSolaceOscillationCommand(0);
  sendSolacePositionCommand(50);
  Serial.println("[TEST] Wave motion completed!");
}

void testSolacePatterns() {
  if (!solaceConnected || !pSolaceWriteChar) {
    Serial.println("[TEST] Solace Pro 2 not connected!");
    return;
  }
  
  Serial.println("[TEST] Testing various patterns...");
  
  // Pattern 1: Quick alternating positions
  Serial.println("[TEST] Pattern 1: Quick alternating...");
  for (int i = 0; i < 10; i++) {
    sendSolacePositionCommand(20);
    delay(500);
    sendSolacePositionCommand(80);
    delay(500);
  }
  
  // Pattern 2: Gradual build-up
  Serial.println("[TEST] Pattern 2: Gradual build-up...");
  for (int intensity = 2; intensity <= 12; intensity += 2) {
    sendSolaceFunctionCommand(30, 70, intensity);
    delay(2000);
  }
  
  // Pattern 3: Random positions with oscillation
  Serial.println("[TEST] Pattern 3: Random movement...");
  for (int i = 0; i < 15; i++) {
    int pos = random(10, 91);
    int osc = random(3, 15);
    sendSolacePositionCommand(pos);
    delay(200);
    sendSolaceOscillationCommand(osc);
    delay(800);
  }
  
  // Stop
  sendSolaceOscillationCommand(0);
  sendSolacePositionCommand(50);
  Serial.println("[TEST] Pattern test completed!");
}
