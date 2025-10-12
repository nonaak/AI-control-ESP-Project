/*
 * Keon Test ESP - TTGO T-Display V1.1 (Simplified Version)
 * 
 * Test programma voor Keon verbinding met visuele feedback
 * - Menu met Keon en Solace verbindingsopties
 * - Simpele rechthoek die meebeweegt met Keon sleeve
 * - Nunchuk besturing op I2C pinnen 21 en 22
 * - TTGO T-Display 1.14 inch (135x240)
 * 
 * Hardware: LilyGO TTGO T-Display V1.1 ESP32
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <NintendoExtensionCtrl.h>
#include <NimBLEDevice.h>

// Display setup
TFT_eSPI tft = TFT_eSPI(); 
Nunchuk nchuk;

// Kiiroo BLE Protocol Constants (from documentation)
// Fleshlight Launch (Kiiroo v2) UUIDs
static NimBLEUUID serviceUUID("88f80580-0000-01e6-aace-0002a5d5c51b");
static NimBLEUUID dataCharUUID("88f80581-0000-01e6-aace-0002a5d5c51b");      // Write characteristic
static NimBLEUUID statusCharUUID("88f80582-0000-01e6-aace-0002a5d5c51b");    // Notify characteristic  
static NimBLEUUID commandCharUUID("88f80583-0000-01e6-aace-0002a5d5c51b");   // Command/Bootloader characteristic

// Alternative Kiiroo Onyx 2 UUIDs (fallback)
static NimBLEUUID altServiceUUID("f60402a6-0294-4bdb-9f20-6758133f7090");
static NimBLEUUID altDataCharUUID("02962ac9-e86f-4094-989d-231d69995fc2");
static NimBLEUUID altStatusCharUUID("d44d0393-0731-43b3-a373-8fc70b1f3323");
static NimBLEUUID altCommandCharUUID("c7b7a04b-2cc4-40ff-8b10-5d531d1161db");

// BLE objects - Keon
NimBLEClient* pKeonClient = nullptr;
NimBLERemoteCharacteristic* pKeonDataChar = nullptr;
NimBLERemoteCharacteristic* pKeonCommandChar = nullptr;
NimBLERemoteCharacteristic* pKeonStatusChar = nullptr;

// BLE objects - Solace  
NimBLEClient* pSolaceClient = nullptr;
NimBLERemoteCharacteristic* pSolaceDataChar = nullptr;
NimBLERemoteCharacteristic* pSolaceCommandChar = nullptr;
NimBLERemoteCharacteristic* pSolaceStatusChar = nullptr;

// Shared BLE scan
NimBLEScan* pBLEScan = nullptr;

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
const uint16_t COL_ORANGE = TFT_ORANGE;
const uint16_t COL_GRAY = TFT_DARKGREY;

// App states
enum AppState {
  STATE_MENU,
  STATE_CONNECTION_POPUP,
  STATE_KEON_DISPLAY
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
bool verboseLogging = false;

// BLE timing
uint32_t lastBLECommand = 0;
const uint32_t BLE_COMMAND_INTERVAL = 50; // 20Hz max for Keon

// Debug tracking structures
struct DeviceInfo {
  NimBLEAddress address;
  std::string name;
  bool isKiiroo;
  int rssi;
};

// Debug variables  
std::vector<DeviceInfo> discoveredDevices;
bool keonDeviceFound = false;
bool solaceDeviceFound = false;
NimBLEAddress keonDeviceAddress;
NimBLEAddress solaceDeviceAddress;

// Current Keon state
uint8_t currentKeonPosition = 50;  // 0-99 (50 = middle)
uint8_t currentKeonSpeed = 0;      // 0-99 (0 = stopped)

// Keon display variables
float sleevePosition = 0.5f;  // 0.0 = top, 1.0 = bottom
float targetPosition = 0.5f;
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

// Function declarations
bool connectToKeonDevice();
bool connectToSolaceDevice();
bool sendKeonCommand(uint8_t position, uint8_t speed);
bool sendSolaceCommand(uint8_t value);
void disconnectKeonDevice();
void disconnectSolaceDevice();
bool scanForKiirooDevices();

// Debug function declarations
void scanAllDevices();
bool scanForKiirooDevicesLong(uint32_t scanTime);
void showDetailedDeviceInfo();
void scanDeviceServices(String addressStr);
void showRSSIInfo();
void showBLEInfo();
void resetBLEAdapter();
void startAdvertising();
void stopAdvertising();
void testBLEBasics();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Keon Test ESP Starting (Simple Version) ===");
  
  // Initialize buttons
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  
  // Initialize display
  tft.init();
  tft.setRotation(0); // Portrait
  tft.fillScreen(COL_BG);
  
  // Startup screen
  showStartupScreen();
  
  // Initialize I2C for nunchuk
  Wire.begin(21, 22);
  Serial.println("[I2C] Initialized on SDA=21, SCL=22");
  
  // Initialize BLE with detailed debugging
  Serial.println("[BLE] Starting BLE initialization...");
  
  // Disable WiFi first to prevent interference
  Serial.println("[BLE] Disabling WiFi for BLE compatibility...");
  WiFi.mode(WIFI_OFF);
  btStop();
  delay(1000);
  
  Serial.println("[BLE] Initializing NimBLE...");
  NimBLEDevice::init("Keon_Test_ESP");
  
  // Set BLE power to maximum
  Serial.println("[BLE] Setting BLE power to maximum...");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9dBm
  
  // Configure scan object
  pBLEScan = NimBLEDevice::getScan();
  if (pBLEScan) {
    Serial.println("[BLE] BLE scan object created successfully");
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(160);
    pBLEScan->setWindow(80);
    pBLEScan->setDuplicateFilter(false);
    bleInitialized = true;
  } else {
    Serial.println("[BLE] ERROR: Failed to create BLE scan object!");
    bleInitialized = false;
    return;
  }
  
  // Display BLE adapter info
  Serial.println("[BLE] === BLE ADAPTER INFO ===");
  Serial.printf("[BLE] Device Name: %s\n", NimBLEDevice::toString().c_str());
  Serial.printf("[BLE] Address: %s\n", NimBLEDevice::getAddress().toString().c_str());
  Serial.printf("[BLE] MTU: %d\n", NimBLEDevice::getMTU());
  Serial.printf("[BLE] Power Level: %d\n", NimBLEDevice::getPower());
  Serial.printf("[BLE] Initialized: %s\n", bleInitialized ? "YES" : "NO");
  Serial.println("[BLE] ========================");
  
  // Hardware specific debug info
  Serial.println("[HARDWARE] === ESP32 INFO ===");
  Serial.printf("[HARDWARE] Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("[HARDWARE] Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("[HARDWARE] CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("[HARDWARE] Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[HARDWARE] Flash Size: %d bytes\n", ESP.getFlashChipSize());
  Serial.println("[HARDWARE] ==================");
  
  Serial.println("[BLE] NimBLE initialized successfully");
  
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
  
  Serial.println("[SETUP] Keon Test ready!");
  Serial.println("\n*** Type 'help' in Serial Monitor for commands! ***");
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
    case STATE_KEON_DISPLAY:
      handleKeonDisplay(input);
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

// ====== DEBUGGING FUNCTIONS ======

void scanAllDevices() {
  if (!bleInitialized) {
    Serial.println("[DEBUG] BLE not initialized!");
    return;
  }
  
  Serial.println("[DEBUG] === SCANNING ALL BLE DEVICES ===");
  
  // Optimized scan parameters
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);  // 62.5ms - faster scanning
  pBLEScan->setWindow(50);     // 31.25ms window
  pBLEScan->setDuplicateFilter(false);
  
  pBLEScan->start(15, false);
  NimBLEScanResults results = pBLEScan->getResults();
  int deviceCount = results.getCount();
  
  Serial.printf("[DEBUG] Found %d total devices:\n", deviceCount);
  Serial.println("[DEBUG] ----------------------------------------");
  
  for (int i = 0; i < deviceCount; i++) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    std::string deviceName = device->getName();
    NimBLEAddress address = device->getAddress();
    int rssi = device->getRSSI();
    
    Serial.printf("[DEBUG] %d. %s\n", i+1, deviceName.length() > 0 ? deviceName.c_str() : "<No Name>");
    Serial.printf("    Address: %s\n", address.toString().c_str());
    Serial.printf("    RSSI: %d dBm\n", rssi);
    Serial.printf("    Address Type: %d\n", device->getAddressType());
    
    if (device->haveManufacturerData()) {
      std::string mfgData = device->getManufacturerData();
      Serial.printf("    Manufacturer Data: ");
      for (int j = 0; j < mfgData.length(); j++) {
        Serial.printf("%02X ", (uint8_t)mfgData[j]);
      }
      Serial.println();
    }
    
    if (device->haveServiceUUID()) {
      Serial.printf("    Service UUID: %s\n", device->getServiceUUID().toString().c_str());
    }
    
    Serial.println();
  }
  
  pBLEScan->clearResults();
  Serial.println("[DEBUG] === END OF DEVICE LIST ===");
}

bool scanForKiirooDevicesLong(uint32_t scanTime) {
  if (!bleInitialized) {
    Serial.println("[DEBUG] BLE not initialized!");
    return false;
  }
  
  Serial.printf("[DEBUG] Starting %d second extra-long scan...\n", scanTime);
  discoveredDevices.clear();
  keonDeviceFound = false;
  solaceDeviceFound = false;
  
  // Optimized scan parameters
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);  // 62.5ms - faster scanning
  pBLEScan->setWindow(50);     // 31.25ms window
  pBLEScan->setDuplicateFilter(false);
  
  pBLEScan->start(15, false);
  NimBLEScanResults results = pBLEScan->getResults();
  int deviceCount = results.getCount();
  
  Serial.printf("[DEBUG] Found %d devices total\n", deviceCount);
  
  // Process each device with extra verbose info
  for (int i = 0; i < deviceCount; i++) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    std::string deviceName = device->getName();
    NimBLEAddress address = device->getAddress();
    
    Serial.printf("[DEBUG] Device %d: '%s' (%s) RSSI: %d\n", i+1, 
                 deviceName.c_str(), address.toString().c_str(), device->getRSSI());
    
    // Check if this looks like a Kiiroo device with verbose logging
    bool isKiiroo = false;
    
    if (deviceName.find("Launch") != std::string::npos ||
        deviceName.find("Keon") != std::string::npos ||
        deviceName.find("Kiiroo") != std::string::npos ||
        deviceName.find("Onyx") != std::string::npos ||
        deviceName.find("Pearl") != std::string::npos ||
        deviceName.find("Solace") != std::string::npos) {
      isKiiroo = true;
      Serial.printf("[DEBUG] *** KIIROO DEVICE DETECTED BY NAME: %s ***\n", deviceName.c_str());
    }
    
    if (device->isAdvertisingService(serviceUUID) ||
        device->isAdvertisingService(altServiceUUID)) {
      isKiiroo = true;
      Serial.printf("[DEBUG] *** KIIROO DEVICE DETECTED BY SERVICE UUID ***\n");
    }
    
    if (isKiiroo) {
      DeviceInfo deviceInfo;
      deviceInfo.address = address;
      deviceInfo.name = deviceName.length() > 0 ? deviceName : "Unknown Kiiroo";
      deviceInfo.isKiiroo = true;
      discoveredDevices.push_back(deviceInfo);
      
      if (!keonDeviceFound && 
          (deviceName.find("Keon") != std::string::npos || 
           deviceName.find("Launch") != std::string::npos)) {
        keonDeviceAddress = address;
        keonDeviceFound = true;
        Serial.printf("[DEBUG] *** SELECTED AS KEON DEVICE ***\n");
      }
      
      if (!solaceDeviceFound && 
          (deviceName.find("Solace") != std::string::npos ||
           deviceName.find("Pearl") != std::string::npos ||
           deviceName.find("Onyx") != std::string::npos)) {
        solaceDeviceAddress = address;
        solaceDeviceFound = true;
        Serial.printf("[DEBUG] *** SELECTED AS SOLACE DEVICE ***\n");
      }
    }
  }
  
  pBLEScan->clearResults();
  
  int kiirooCount = 0;
  for (auto& dev : discoveredDevices) {
    if (dev.isKiiroo) kiirooCount++;
  }
  
  Serial.printf("[DEBUG] Found %d Kiiroo device(s) total\n", kiirooCount);
  Serial.printf("[DEBUG] Keon device found: %s\n", keonDeviceFound ? "YES" : "NO");
  Serial.printf("[DEBUG] Solace device found: %s\n", solaceDeviceFound ? "YES" : "NO");
  
  return kiirooCount > 0;
}

void showDetailedDeviceInfo() {
  Serial.println("[DEBUG] === DETAILED DEVICE INFORMATION ===");
  
  if (discoveredDevices.empty()) {
    Serial.println("[DEBUG] No devices found yet. Run 'scan' or 'scanall' first.");
    return;
  }
  
  for (int i = 0; i < discoveredDevices.size(); i++) {
    auto& dev = discoveredDevices[i];
    Serial.printf("[DEBUG] Device %d:\n", i+1);
    Serial.printf("  Name: %s\n", dev.name.c_str());
    Serial.printf("  Address: %s\n", dev.address.toString().c_str());
    Serial.printf("  Is Kiiroo: %s\n", dev.isKiiroo ? "YES" : "NO");
    Serial.println();
  }
  
  Serial.printf("[DEBUG] Current selections:\n");
  if (keonDeviceFound) {
    Serial.printf("  Keon: %s\n", keonDeviceAddress.toString().c_str());
  } else {
    Serial.println("  Keon: Not found");
  }
  
  if (solaceDeviceFound) {
    Serial.printf("  Solace: %s\n", solaceDeviceAddress.toString().c_str());
  } else {
    Serial.println("  Solace: Not found");
  }
}

void scanDeviceServices(String addressStr) {
  Serial.printf("[DEBUG] Connecting to %s to scan services...\n", addressStr.c_str());
  
  NimBLEAddress address(addressStr.c_str(), BLE_ADDR_PUBLIC);
  
  NimBLEClient* pTempClient = NimBLEDevice::createClient();
  if (!pTempClient) {
    Serial.println("[DEBUG] Failed to create temporary client");
    return;
  }
  
  pTempClient->setConnectTimeout(15);
  
  if (!pTempClient->connect(address)) {
    Serial.println("[DEBUG] Failed to connect to device for service scan");
    NimBLEDevice::deleteClient(pTempClient);
    return;
  }
  
  Serial.println("[DEBUG] Connected! Discovering services...");
  
  auto pServices = pTempClient->getServices(true);
  
  if (pServices.size() == 0) {
    Serial.println("[DEBUG] No services found");
  } else {
    Serial.printf("[DEBUG] Found %d services:\n", pServices.size());
    
    for (int i = 0; i < pServices.size(); i++) {
      NimBLERemoteService* pService = pServices[i];
      Serial.printf("[DEBUG]   Service %d: %s\n", i+1, pService->getUUID().toString().c_str());
      
      auto pChars = pService->getCharacteristics(true);
      for (int j = 0; j < pChars.size(); j++) {
        NimBLERemoteCharacteristic* pChar = pChars[j];
        Serial.printf("[DEBUG]     Char %d: %s (Props: ", j+1, pChar->getUUID().toString().c_str());
        if (pChar->canRead()) Serial.print("R");
        if (pChar->canWrite()) Serial.print("W");
        if (pChar->canNotify()) Serial.print("N");
        if (pChar->canIndicate()) Serial.print("I");
        Serial.println(")");
        
        // Check if this matches Kiiroo characteristics
        if (pChar->getUUID() == dataCharUUID || pChar->getUUID() == altDataCharUUID) {
          Serial.println("[DEBUG]       *** KIIROO DATA CHARACTERISTIC ***");
        }
        if (pChar->getUUID() == commandCharUUID || pChar->getUUID() == altCommandCharUUID) {
          Serial.println("[DEBUG]       *** KIIROO COMMAND CHARACTERISTIC ***");
        }
        if (pChar->getUUID() == statusCharUUID || pChar->getUUID() == altStatusCharUUID) {
          Serial.println("[DEBUG]       *** KIIROO STATUS CHARACTERISTIC ***");
        }
      }
    }
  }
  
  pTempClient->disconnect();
  NimBLEDevice::deleteClient(pTempClient);
  Serial.println("[DEBUG] Service scan complete");
}

void showRSSIInfo() {
  Serial.println("[DEBUG] === SIGNAL STRENGTH INFO ===");
  // This would require storing RSSI during scan, simplified for now
  Serial.println("[DEBUG] Run 'scanall' to see RSSI values for all devices");
}

void showBLEInfo() {
  Serial.println("[DEBUG] === BLE ADAPTER INFO ===");
  Serial.printf("[DEBUG] BLE Initialized: %s\n", bleInitialized ? "YES" : "NO");
  Serial.printf("[DEBUG] Device Name: %s\n", NimBLEDevice::toString().c_str());
  Serial.printf("[DEBUG] Address: %s\n", NimBLEDevice::getAddress().toString().c_str());
  Serial.printf("[DEBUG] MTU: %d\n", NimBLEDevice::getMTU());
  Serial.printf("[DEBUG] Power Level: %d\n", NimBLEDevice::getPower());
  
  Serial.println("[DEBUG] Target Kiiroo Service UUIDs:");
  Serial.printf("[DEBUG]   Primary: %s\n", serviceUUID.toString().c_str());
  Serial.printf("[DEBUG]   Alt: %s\n", altServiceUUID.toString().c_str());
  
  Serial.println("[DEBUG] Target Kiiroo Characteristic UUIDs:");
  Serial.printf("[DEBUG]   Data Primary: %s\n", dataCharUUID.toString().c_str());
  Serial.printf("[DEBUG]   Data Alt: %s\n", altDataCharUUID.toString().c_str());
  Serial.printf("[DEBUG]   Command Primary: %s\n", commandCharUUID.toString().c_str());
  Serial.printf("[DEBUG]   Command Alt: %s\n", altCommandCharUUID.toString().c_str());
}

void resetBLEAdapter() {
  Serial.println("[DEBUG] Resetting BLE adapter...");
  
  // Disconnect all clients first
  if (keonConnected && pKeonClient) {
    disconnectKeonDevice();
  }
  if (solaceConnected && pSolaceClient) {
    disconnectSolaceDevice();
  }
  
  // Stop advertising if running
  stopAdvertising();
  
  // Clear discovered devices
  discoveredDevices.clear();
  keonDeviceFound = false;
  solaceDeviceFound = false;
  
  // Deinitialize and reinitialize NimBLE
  Serial.println("[DEBUG] Deinitializing BLE...");
  NimBLEDevice::deinit(true);
  delay(2000);
  
  Serial.println("[DEBUG] Reinitializing BLE...");
  NimBLEDevice::init("Keon_Test_ESP");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  pBLEScan = NimBLEDevice::getScan();
  if (pBLEScan) {
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(160);
    pBLEScan->setWindow(80);
    pBLEScan->setDuplicateFilter(false);
    bleInitialized = true;
  } else {
    bleInitialized = false;
  }
  
  Serial.println("[DEBUG] BLE adapter reset complete");
}

void startAdvertising() {
  if (!bleInitialized) {
    Serial.println("[DEBUG] BLE not initialized - cannot start advertising");
    return;
  }
  
  Serial.println("[DEBUG] Starting BLE advertising...");
  
  // Get advertising object
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  
  if (!pAdvertising) {
    Serial.println("[DEBUG] Failed to get advertising object");
    return;
  }
  
  // Configure advertising data
  pAdvertising->setName("Keon_Test_ESP");
  pAdvertising->setManufacturerData("ESP32_TEST");
  
  // Set advertising parameters for better visibility
  pAdvertising->setMinInterval(0x20);  // 20ms
  pAdvertising->setMaxInterval(0x40);  // 40ms
  
  // Add service UUID to make it discoverable
  pAdvertising->addServiceUUID(NimBLEUUID("12345678-1234-1234-1234-123456789abc"));
  
  // Start advertising
  if (pAdvertising->start()) {
    Serial.println("[DEBUG] BLE advertising started successfully!");
    Serial.println("[DEBUG] ESP32 should now be visible to other devices");
    Serial.printf("[DEBUG] Device name: %s\n", "Keon_Test_ESP");
    Serial.printf("[DEBUG] Address: %s\n", NimBLEDevice::getAddress().toString().c_str());
  } else {
    Serial.println("[DEBUG] Failed to start BLE advertising");
  }
}

void stopAdvertising() {
  Serial.println("[DEBUG] Stopping BLE advertising...");
  
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising) {
    pAdvertising->stop();
    Serial.println("[DEBUG] BLE advertising stopped");
  }
}

void testBLEBasics() {
  Serial.println("[DEBUG] === BLE BASIC FUNCTIONALITY TEST ===");
  
  if (!bleInitialized) {
    Serial.println("[DEBUG] ERROR: BLE not initialized!");
    return;
  }
  
  Serial.printf("[DEBUG] BLE Initialized: %s\n", bleInitialized ? "YES" : "NO");
  Serial.printf("[DEBUG] Device Address: %s\n", NimBLEDevice::getAddress().toString().c_str());
  Serial.printf("[DEBUG] Power Level: %d\n", NimBLEDevice::getPower());
  
  // Test scan object
  if (pBLEScan) {
    Serial.println("[DEBUG] Scan object: OK");
  } else {
    Serial.println("[DEBUG] Scan object: FAILED");
  }
  
  // Test advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising) {
    Serial.println("[DEBUG] Advertising object: OK");
  } else {
    Serial.println("[DEBUG] Advertising object: FAILED");
  }
  
  Serial.println("[DEBUG] === Starting 5 second advertising test ===");
  startAdvertising();
  Serial.println("[DEBUG] Check your phone now - ESP32 should be visible as 'Keon_Test_ESP'");
  delay(5000);
  stopAdvertising();
  Serial.println("[DEBUG] Advertising test complete");
  
  Serial.println("[DEBUG] === BLE BASIC TEST COMPLETE ===");
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
      Serial.printf("[MENU] Selected: %s\n", menuIndex == 0 ? "Keon" : "Solace");
    }
  }
  
  // Button navigation
  if (now - lastButtonTime > BUTTON_DEBOUNCE) {
    if (input.leftPressed) {
      menuIndex = (menuIndex == 0) ? 1 : 0;
      lastButtonTime = now;
      drawMenu();
      Serial.printf("[MENU] Button nav: %s\n", menuIndex == 0 ? "Keon" : "Solace");
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

void handleKeonDisplay(InputState input) {
  static bool cPrev = false;
  
  // Update sleeve position based on nunchuk Y
  if (nunchukReady) {
    // Map joystick Y (0-255) to position (0.0-1.0)
    targetPosition = 1.0f - ((float)input.jy / 255.0f);
    targetPosition = constrain(targetPosition, 0.0f, 1.0f);
    
    // Convert to Keon position (0-99) and calculate speed based on joystick X
    uint8_t newPosition = (uint8_t)(targetPosition * 99.0f);
    uint8_t newSpeed = 0;
    
    // Use joystick X for speed control (center = 0, edges = max speed)
    int jx_centered = input.jx - 128;
    if (abs(jx_centered) > 20) { // Deadzone
      newSpeed = (uint8_t)(abs(jx_centered) * 99 / 127);
      newSpeed = constrain(newSpeed, 0, 99);
    }
    
    // Send command to Keon if connected and values changed
    if (keonConnected && 
        (newPosition != currentKeonPosition || newSpeed != currentKeonSpeed) &&
        (millis() - lastBLECommand >= BLE_COMMAND_INTERVAL)) {
      
      if (sendKeonCommand(newPosition, newSpeed)) {
        currentKeonPosition = newPosition;
        currentKeonSpeed = newSpeed;
        lastBLECommand = millis();
      }
    }
  }
  
  // C button - return to menu
  if (input.c && !cPrev) {
    // Stop Keon movement before returning to menu
    if (keonConnected) {
      sendKeonCommand(currentKeonPosition, 0); // Stop movement
      currentKeonSpeed = 0;
    }
    currentState = STATE_MENU;
    drawMenu();
    Serial.println("[KEON] Returned to menu");
  }
  
  cPrev = input.c;
}

void executeConnection() {
  if (popupDevice == 0) { // Keon
    if (keonConnected) {
      // Disconnect
      disconnectKeonDevice();
      keonConnected = false;
      Serial.println("[CONNECTION] Keon disconnected");
    } else {
      // Try to connect
      Serial.println("[CONNECTION] Attempting to connect to Keon...");
      bool success = connectToKeonDevice();
      if (success) {
        keonConnected = true;
        showConnectionAnimation("Keon");
        currentState = STATE_KEON_DISPLAY;
        drawKeonDisplay();
        Serial.println("[KEON] Connected successfully - switched to display mode");
      } else {
        showConnectionFailure("Keon");
        Serial.println("[KEON] Connection failed - device not found");
      }
    }
  } else { // Solace
    if (solaceConnected) {
      // Disconnect
      disconnectSolaceDevice();
      solaceConnected = false;
      Serial.println("[CONNECTION] Solace disconnected");
    } else {
      // Try to connect
      Serial.println("[CONNECTION] Attempting to connect to Solace...");
      bool success = connectToSolaceDevice();
      if (success) {
        solaceConnected = true;
        showConnectionAnimation("Solace");
        Serial.println("[SOLACE] Connected successfully");
      } else {
        showConnectionFailure("Solace");
        Serial.println("[SOLACE] Connection failed - device not found");
      }
    }
  }
}

void openConnectionPopup() {
  popupDevice = menuIndex;
  popupChoice = 0;
  currentState = STATE_CONNECTION_POPUP;
  drawPopup();
  Serial.printf("[POPUP] Opened for %s\n", menuIndex == 0 ? "Keon" : "Solace");
}

void closePopup() {
  currentState = STATE_MENU;
  popupDevice = -1;
  popupChoice = 0;
  drawMenu();
  Serial.println("[POPUP] Closed");
}

void updateAnimation() {
  if (currentState == STATE_KEON_DISPLAY) {
    // Smooth animation towards target
    float diff = targetPosition - sleevePosition;
    if (abs(diff) > 0.01f) {
      sleevePosition += diff * 0.1f; // Smooth interpolation
      drawKeonDisplay();
    }
  }
}

void showStartupScreen() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(20, 60);
  tft.println("KIIROO TEST");
  
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("TTGO T-Display V1.1");
  tft.setCursor(10, 120);
  tft.println("Keon + Solace");
  tft.setCursor(10, 140);
  tft.println("Initializing...");
  
  Serial.println("[DISPLAY] Startup screen shown");
}

// ====== BLE FUNCTIONS (REAL KEON SUPPORT) ======

// BLE Client Callbacks for Keon
class KeonClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    Serial.println("[KEON] BLE client connected");
  }
  
  void onDisconnect(NimBLEClient* pClient) {
    Serial.println("[KEON] BLE client disconnected");
    keonConnected = false;
    pKeonClient = nullptr;
    pKeonDataChar = nullptr;
    pKeonCommandChar = nullptr;
    pKeonStatusChar = nullptr;
  }
  
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    Serial.println("[KEON] Connection parameters update request");
    return true;  // Accept the update
  }
};

// BLE Client Callbacks for Solace
class SolaceClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    Serial.println("[SOLACE] BLE client connected");
  }
  
  void onDisconnect(NimBLEClient* pClient) {
    Serial.println("[SOLACE] BLE client disconnected");
    solaceConnected = false;
    pSolaceClient = nullptr;
    pSolaceDataChar = nullptr;
    pSolaceCommandChar = nullptr;
    pSolaceStatusChar = nullptr;
  }
  
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    Serial.println("[SOLACE] Connection parameters update request");
    return true;  // Accept the update
  }
};

// Global variables for discovered devices (already declared above)

bool scanForKiirooDevices() {
  if (!bleInitialized) {
    Serial.println("[BLE] BLE not initialized!");
    return false;
  }
  
  Serial.println("[BLE] Scanning for Kiiroo devices...");
  discoveredDevices.clear();
  keonDeviceFound = false;
  solaceDeviceFound = false;
  
  // Optimized scan parameters for better device discovery
  pBLEScan->setActiveScan(true);    // Active scan for device names and extra info
  pBLEScan->setInterval(100);       // 62.5ms intervals (100 * 0.625ms) - faster scanning
  pBLEScan->setWindow(50);          // 31.25ms scan window (50 * 0.625ms) 
  pBLEScan->setDuplicateFilter(false);  // Allow duplicate results for RSSI updates
  
  Serial.println("[BLE] Scan parameters configured:");
  Serial.println("[BLE]   - Active scan: ON (gets device names)");
  Serial.println("[BLE]   - Interval: 62.5ms (fast scanning)");
  Serial.println("[BLE]   - Window: 31.25ms");
  Serial.println("[BLE]   - Duplicates: ALLOWED");
  
  Serial.println("[BLE] Starting 15 second scan...");
  
  // Use blocking scan with longer timeout
  pBLEScan->start(15, false);  // 15 second scan for better device discovery
  
  // Get scan results
  NimBLEScanResults results = pBLEScan->getResults();
  int deviceCount = results.getCount();
  
  Serial.printf("[BLE] Found %d devices total\n", deviceCount);
  
  // Process each device
  for (int i = 0; i < deviceCount; i++) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    std::string deviceName = device->getName();
    NimBLEAddress address = device->getAddress();
    
    Serial.printf("[BLE] Device %d: %s (%s)\n", i+1, 
                 deviceName.c_str(), address.toString().c_str());
    
    // Check if this looks like a Kiiroo device
    bool isKiiroo = false;
    
    // Check by name
    if (deviceName.find("Launch") != std::string::npos ||
        deviceName.find("Keon") != std::string::npos ||
        deviceName.find("Kiiroo") != std::string::npos ||
        deviceName.find("Onyx") != std::string::npos ||
        deviceName.find("Pearl") != std::string::npos ||
        deviceName.find("Solace") != std::string::npos) {
      isKiiroo = true;
      Serial.printf("[BLE] Found Kiiroo device by name: %s\n", deviceName.c_str());
    }
    
    // Check for Kiiroo service UUIDs
    if (device->isAdvertisingService(serviceUUID) ||
        device->isAdvertisingService(altServiceUUID)) {
      isKiiroo = true;
      Serial.printf("[BLE] Found Kiiroo device by service UUID: %s\n", deviceName.c_str());
    }
    
    if (isKiiroo) {
      DeviceInfo deviceInfo;
      deviceInfo.address = address;
      deviceInfo.name = deviceName.length() > 0 ? deviceName : "Unknown Kiiroo";
      deviceInfo.isKiiroo = true;
      discoveredDevices.push_back(deviceInfo);
      
      // Save first Keon device found
      if (!keonDeviceFound && 
          (deviceName.find("Keon") != std::string::npos || 
           deviceName.find("Launch") != std::string::npos)) {
        keonDeviceAddress = address;
        keonDeviceFound = true;
        Serial.printf("[BLE] Selected Keon device: %s\n", deviceName.c_str());
      }
      
      // Save first Solace device found
      if (!solaceDeviceFound && 
          (deviceName.find("Solace") != std::string::npos ||
           deviceName.find("Pearl") != std::string::npos ||
           deviceName.find("Onyx") != std::string::npos)) {
        solaceDeviceAddress = address;
        solaceDeviceFound = true;
        Serial.printf("[BLE] Selected Solace device: %s\n", deviceName.c_str());
      }
    }
  }
  
  pBLEScan->clearResults();
  
  int kiirooCount = 0;
  for (auto& dev : discoveredDevices) {
    if (dev.isKiiroo) kiirooCount++;
  }
  
  Serial.printf("[BLE] Found %d Kiiroo device(s)\n", kiirooCount);
  return kiirooCount > 0;
}

bool connectToKeonDevice() {
  Serial.println("[BLE] Starting real Keon connection attempt...");
  
  if (!bleInitialized) {
    Serial.println("[BLE] BLE not initialized!");
    return false;
  }
  
  // First scan for devices if we haven't found a Keon yet
  if (!keonDeviceFound) {
    Serial.println("[BLE] No Keon device found yet, scanning...");
    if (!scanForKiirooDevices()) {
      Serial.println("[BLE] No Kiiroo devices found during scan");
      return false;
    }
  }
  
  if (!keonDeviceFound) {
    Serial.println("[BLE] No Keon device available for connection");
    return false;
  }
  
  Serial.printf("[BLE] Attempting to connect to Keon at %s\n", 
               keonDeviceAddress.toString().c_str());
  
  // Create BLE client
  pKeonClient = NimBLEDevice::createClient();
  if (!pKeonClient) {
    Serial.println("[BLE] Failed to create BLE client");
    return false;
  }
  
  // Set connection parameters
  pKeonClient->setClientCallbacks(new KeonClientCallbacks());
  
  // Set connection timeout
  pKeonClient->setConnectTimeout(30);  // 30 second timeout
  
  // Connect to the Keon device with retry
  Serial.println("[BLE] Connecting to Keon device (may take up to 30 seconds per attempt)...");
  
  bool connected = false;
  for (int attempt = 1; attempt <= 10; attempt++) {
    Serial.printf("[BLE] Connection attempt %d/10\n", attempt);
    
    if (pKeonClient->connect(keonDeviceAddress)) {
      connected = true;
      break;
    }
    
    if (attempt < 10) {
      Serial.printf("[BLE] Attempt %d failed, retrying in 5 seconds...\n", attempt);
      delay(5000);
    }
  }
  
  if (!connected) {
    Serial.println("[BLE] Failed to connect to Keon device - check if it's in pairing mode");
    NimBLEDevice::deleteClient(pKeonClient);
    pKeonClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Connected! Discovering services...");
  
  // Try primary Kiiroo service first
  NimBLERemoteService* pService = pKeonClient->getService(serviceUUID);
  if (!pService) {
    Serial.println("[BLE] Primary service not found, trying alternative...");
    pService = pKeonClient->getService(altServiceUUID);
  }
  
  if (!pService) {
    Serial.println("[BLE] No Kiiroo service found!");
    pKeonClient->disconnect();
    NimBLEDevice::deleteClient(pKeonClient);
    pKeonClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Kiiroo service found! Getting characteristics...");
  
  // Get data characteristic (most important for commands)
  pKeonDataChar = pService->getCharacteristic(dataCharUUID);
  if (!pKeonDataChar) {
    pKeonDataChar = pService->getCharacteristic(altDataCharUUID);
  }
  
  if (!pKeonDataChar) {
    Serial.println("[BLE] Data characteristic not found!");
    pKeonClient->disconnect();
    NimBLEDevice::deleteClient(pKeonClient);
    pKeonClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Data characteristic found!");
  
  // Get optional characteristics
  pKeonCommandChar = pService->getCharacteristic(commandCharUUID);
  if (!pKeonCommandChar) {
    pKeonCommandChar = pService->getCharacteristic(altCommandCharUUID);
  }
  
  pKeonStatusChar = pService->getCharacteristic(statusCharUUID);
  if (!pKeonStatusChar) {
    pKeonStatusChar = pService->getCharacteristic(altStatusCharUUID);
  }
  
  // Initialize device (send 0x00 to command characteristic if available)
  if (pKeonCommandChar && pKeonCommandChar->canWrite()) {
    Serial.println("[BLE] Sending initialization command...");
    uint8_t initCmd = 0x00;
    if (pKeonCommandChar->writeValue(&initCmd, 1)) {
      Serial.println("[BLE] Initialization command sent successfully");
    } else {
      Serial.println("[BLE] Failed to send initialization command");
    }
  }
  
  // Enable notifications on status characteristic if available
  if (pKeonStatusChar && pKeonStatusChar->canNotify()) {
    Serial.println("[BLE] Enabling status notifications...");
    pKeonStatusChar->subscribe(true);
  }
  
  Serial.println("[BLE] Keon connected and initialized successfully!");
  return true;
}

bool connectToSolaceDevice() {
  Serial.println("[BLE] Starting real Solace connection attempt...");
  
  if (!bleInitialized) {
    Serial.println("[BLE] BLE not initialized!");
    return false;
  }
  
  // First scan for devices if we haven't found a Solace yet
  if (!solaceDeviceFound) {
    Serial.println("[BLE] No Solace device found yet, scanning...");
    if (!scanForKiirooDevices()) {
      Serial.println("[BLE] No Kiiroo devices found during scan");
      return false;
    }
  }
  
  if (!solaceDeviceFound) {
    Serial.println("[BLE] No Solace device available for connection");
    return false;
  }
  
  Serial.printf("[BLE] Attempting to connect to Solace at %s\n", 
               solaceDeviceAddress.toString().c_str());
  
  // Create BLE client
  pSolaceClient = NimBLEDevice::createClient();
  if (!pSolaceClient) {
    Serial.println("[BLE] Failed to create BLE client for Solace");
    return false;
  }
  
  // Set connection parameters
  pSolaceClient->setClientCallbacks(new SolaceClientCallbacks());
  
  // Set connection timeout
  pSolaceClient->setConnectTimeout(30);  // 30 second timeout
  
  // Connect to the Solace device with retry
  Serial.println("[BLE] Connecting to Solace device (may take up to 30 seconds per attempt)...");
  
  bool connected = false;
  for (int attempt = 1; attempt <= 10; attempt++) {
    Serial.printf("[BLE] Connection attempt %d/10\n", attempt);
    
    if (pSolaceClient->connect(solaceDeviceAddress)) {
      connected = true;
      break;
    }
    
    if (attempt < 10) {
      Serial.printf("[BLE] Attempt %d failed, retrying in 5 seconds...\n", attempt);
      delay(5000);
    }
  }
  
  if (!connected) {
    Serial.println("[BLE] Failed to connect to Solace device - check if it's in pairing mode");
    NimBLEDevice::deleteClient(pSolaceClient);
    pSolaceClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Connected to Solace! Discovering services...");
  
  // Try primary Kiiroo service first
  NimBLERemoteService* pService = pSolaceClient->getService(serviceUUID);
  if (!pService) {
    Serial.println("[BLE] Primary service not found, trying alternative...");
    pService = pSolaceClient->getService(altServiceUUID);
  }
  
  if (!pService) {
    Serial.println("[BLE] No Kiiroo service found on Solace!");
    pSolaceClient->disconnect();
    NimBLEDevice::deleteClient(pSolaceClient);
    pSolaceClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Kiiroo service found on Solace! Getting characteristics...");
  
  // Get data characteristic (most important for commands)
  pSolaceDataChar = pService->getCharacteristic(dataCharUUID);
  if (!pSolaceDataChar) {
    pSolaceDataChar = pService->getCharacteristic(altDataCharUUID);
  }
  
  if (!pSolaceDataChar) {
    Serial.println("[BLE] Data characteristic not found on Solace!");
    pSolaceClient->disconnect();
    NimBLEDevice::deleteClient(pSolaceClient);
    pSolaceClient = nullptr;
    return false;
  }
  
  Serial.println("[BLE] Solace data characteristic found!");
  
  // Get optional characteristics
  pSolaceCommandChar = pService->getCharacteristic(commandCharUUID);
  if (!pSolaceCommandChar) {
    pSolaceCommandChar = pService->getCharacteristic(altCommandCharUUID);
  }
  
  pSolaceStatusChar = pService->getCharacteristic(statusCharUUID);
  if (!pSolaceStatusChar) {
    pSolaceStatusChar = pService->getCharacteristic(altStatusCharUUID);
  }
  
  // Initialize device (send 0x00 to command characteristic if available)
  if (pSolaceCommandChar && pSolaceCommandChar->canWrite()) {
    Serial.println("[BLE] Sending Solace initialization command...");
    uint8_t initCmd = 0x00;
    if (pSolaceCommandChar->writeValue(&initCmd, 1)) {
      Serial.println("[BLE] Solace initialization command sent successfully");
    } else {
      Serial.println("[BLE] Failed to send Solace initialization command");
    }
  }
  
  // Enable notifications on status characteristic if available
  if (pSolaceStatusChar && pSolaceStatusChar->canNotify()) {
    Serial.println("[BLE] Enabling Solace status notifications...");
    pSolaceStatusChar->subscribe(true);
  }
  
  Serial.println("[BLE] Solace connected and initialized successfully!");
  return true;
}

bool sendKeonCommand(uint8_t position, uint8_t speed) {
  if (!keonConnected || !pKeonClient || !pKeonDataChar) {
    Serial.println("[KEON] Not connected or missing data characteristic");
    return false;
  }
  
  if (!pKeonClient->isConnected()) {
    Serial.println("[KEON] BLE client disconnected");
    keonConnected = false;
    return false;
  }
  
  if (!pKeonDataChar->canWrite()) {
    Serial.println("[KEON] Data characteristic not writable");
    return false;
  }
  
  // Kiiroo v2 protocol: 2 bytes [position, speed] both 0-99
  uint8_t command[2] = {position, speed};
  
  // Send command using writeValue (no response)
  bool success = pKeonDataChar->writeValue(command, 2, false);
  
  if (success) {
    Serial.printf("[KEON] Command sent: pos=%d, speed=%d\n", position, speed);
  } else {
    Serial.printf("[KEON] Failed to send command: pos=%d, speed=%d\n", position, speed);
  }
  
  return success;
}

bool sendSolaceCommand(uint8_t value) {
  if (!solaceConnected || !pSolaceClient || !pSolaceDataChar) {
    Serial.println("[SOLACE] Not connected or missing data characteristic");
    return false;
  }
  
  if (!pSolaceClient->isConnected()) {
    Serial.println("[SOLACE] BLE client disconnected");
    solaceConnected = false;
    return false;
  }
  
  if (!pSolaceDataChar->canWrite()) {
    Serial.println("[SOLACE] Data characteristic not writable");
    return false;
  }
  
  // Solace protocol: single byte value (0-99)
  uint8_t command[1] = {value};
  
  // Send command using writeValue (no response)
  bool success = pSolaceDataChar->writeValue(command, 1, false);
  
  if (success) {
    Serial.printf("[SOLACE] Command sent: value=%d\n", value);
  } else {
    Serial.printf("[SOLACE] Failed to send command: value=%d\n", value);
  }
  
  return success;
}

void disconnectKeonDevice() {
  if (pKeonClient) {
    Serial.println("[BLE] Disconnecting from Keon...");
    
    // Send stop command before disconnecting
    if (pKeonDataChar && pKeonDataChar->canWrite()) {
      uint8_t stopCommand[2] = {50, 0};  // Middle position, zero speed
      pKeonDataChar->writeValue(stopCommand, 2, false);
      delay(100);
    }
    
    if (pKeonClient->isConnected()) {
      pKeonClient->disconnect();
    }
    
    NimBLEDevice::deleteClient(pKeonClient);
    pKeonClient = nullptr;
    pKeonDataChar = nullptr;
    pKeonCommandChar = nullptr;
    pKeonStatusChar = nullptr;
    
    Serial.println("[BLE] Disconnected from Keon");
  }
  
  keonConnected = false;
  keonDeviceFound = false;
}

void disconnectSolaceDevice() {
  if (pSolaceClient) {
    Serial.println("[BLE] Disconnecting from Solace...");
    
    // Send stop command before disconnecting (0 = stop)
    if (pSolaceDataChar && pSolaceDataChar->canWrite()) {
      uint8_t stopCommand[1] = {0};  // Stop value
      pSolaceDataChar->writeValue(stopCommand, 1, false);
      delay(100);
    }
    
    if (pSolaceClient->isConnected()) {
      pSolaceClient->disconnect();
    }
    
    NimBLEDevice::deleteClient(pSolaceClient);
    pSolaceClient = nullptr;
    pSolaceDataChar = nullptr;
    pSolaceCommandChar = nullptr;
    pSolaceStatusChar = nullptr;
    
    Serial.println("[BLE] Disconnected from Solace");
  }
  
  solaceConnected = false;
  solaceDeviceFound = false;
}

// ====== DISPLAY FUNCTIONS ======

void showConnectionAnimation(const char* device) {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.println("Connecting");
  
  tft.setTextColor(COL_TEXT);
  tft.setCursor(10, 110);
  tft.println(device);
  
  // Animation dots
  for (int i = 0; i < 3; i++) {
    tft.setTextColor(COL_ORANGE);
    tft.setCursor(10 + i * 15, 140);
    tft.println(".");
    delay(300);
  }
  
  tft.setTextColor(COL_GREEN);
  tft.setCursor(10, 170);
  tft.println("Connected!");
  delay(1000);
}

void showConnectionFailure(const char* device) {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.println("Connecting");
  
  tft.setTextColor(COL_TEXT);
  tft.setCursor(10, 90);
  tft.println(device);
  
  // Animation dots
  for (int i = 0; i < 3; i++) {
    tft.setTextColor(COL_ORANGE);
    tft.setCursor(10 + i * 15, 120);
    tft.println(".");
    delay(300);
  }
  
  // Failure message
  tft.setTextColor(COL_RED);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.println("FAILED!");
  
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 180);
  tft.println("Device not found.");
  tft.setCursor(10, 195);
  tft.println("Check pairing mode");
  
  delay(3000);
}

void drawMenu() {
  tft.fillScreen(COL_BG);
  
  // Title
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KEON TEST");
  
  // Menu items
  for (int i = 0; i < 2; i++) {
    int y = 50 + i * 40;
    bool selected = (i == menuIndex);
    bool connected = (i == 0) ? keonConnected : solaceConnected;
    
    // Selection highlight
    if (selected) {
      tft.fillRect(5, y - 5, 125, 35, COL_GRAY);
    }
    
    // Text
    tft.setTextColor(selected ? COL_SELECTED : COL_TEXT);
    tft.setTextSize(2);
    tft.setCursor(10, y);
    tft.println(i == 0 ? "Keon" : "Solace");
    
    // Status dot
    uint16_t dotColor = connected ? COL_GREEN : COL_RED;
    tft.fillCircle(110, y + 8, 8, dotColor);
    tft.drawCircle(110, y + 8, 8, COL_TEXT);
  }
  
  // Status info
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(10, 140);
  tft.printf("Keon: %s", keonConnected ? "CONNECTED" : "OFF");
  tft.setCursor(10, 155);
  tft.printf("Solace: %s", solaceConnected ? "CONNECTED" : "OFF");
  tft.setCursor(10, 170);
  tft.printf("Nunchuk: %s", nunchukReady ? "OK" : "FAIL");
  
  // Help text
  tft.setTextColor(COL_GRAY);
  tft.setCursor(5, 200);
  tft.println("JY/LEFT: navigate");
  tft.setCursor(5, 215);
  tft.println("Z/RIGHT: select");
}

void drawPopup() {
  // Popup overlay
  int px = 15, py = 70, pw = 105, ph = 100;
  
  tft.fillRoundRect(px, py, pw, ph, 8, COL_BLUE);
  tft.drawRoundRect(px, py, pw, ph, 8, COL_TEXT);
  
  // Title
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(px + 5, py + 10);
  
  const char* device = (popupDevice == 0) ? "Keon" : "Solace";
  bool connected = (popupDevice == 0) ? keonConnected : solaceConnected;
  
  if (connected) {
    tft.println("Disconnect");
  } else {
    tft.println("Connect to");
  }
  
  tft.setTextSize(2);
  tft.setCursor(px + 5, py + 25);
  tft.println(device);
  tft.setTextSize(1);
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

void drawKeonDisplay() {
  tft.fillScreen(COL_BG);
  
  // Title
  tft.setTextColor(COL_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KEON LIVE");
  
  // Sleeve track (vertical line)
  int trackX = 60;
  int trackTop = 50;
  int trackHeight = 140;
  int trackBottom = trackTop + trackHeight;
  
  tft.drawLine(trackX, trackTop, trackX, trackBottom, COL_GRAY);
  tft.drawLine(trackX + 1, trackTop, trackX + 1, trackBottom, COL_GRAY);
  
  // Sleeve rectangle
  int sleeveHeight = 30;
  int sleeveWidth = 20;
  int sleeveY = trackTop + (int)(sleevePosition * (trackHeight - sleeveHeight));
  int sleeveX = trackX - sleeveWidth / 2;
  
  // Clear previous position (simple method)
  tft.fillRect(sleeveX - 2, trackTop - 2, sleeveWidth + 4, trackHeight + 4, COL_BG);
  
  // Redraw track
  tft.drawLine(trackX, trackTop, trackX, trackBottom, COL_GRAY);
  tft.drawLine(trackX + 1, trackTop, trackX + 1, trackBottom, COL_GRAY);
  
  // Draw sleeve
  tft.fillRect(sleeveX, sleeveY, sleeveWidth, sleeveHeight, COL_ORANGE);
  tft.drawRect(sleeveX, sleeveY, sleeveWidth, sleeveHeight, COL_TEXT);
  
  // Position info
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 200);
  tft.printf("Position: %.2f", sleevePosition);
  tft.setCursor(10, 215);
  tft.printf("Target: %.2f", targetPosition);
  
  // Instructions
  tft.setTextColor(COL_GRAY);
  tft.setCursor(5, SCREEN_H - 15);
  tft.println("JY: move sleeve  C: menu");
}

// ====== SERIAL COMMANDS ======

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "help" || command == "?") {
      Serial.println("\n=== KEON/SOLACE TEST - COMMANDS ===");
      Serial.println("Basic Commands:");
      Serial.println("  scan             - Scan for Kiiroo devices");
      Serial.println("  keon             - Connect to Keon");
      Serial.println("  solace           - Connect to Solace");
      Serial.println("  disconnect       - Disconnect all devices");
      Serial.println("  status           - Show connection status");
      Serial.println("  move <p> <s>     - Move Keon to position p with speed s");
      Serial.println("  solace <val>     - Send value to Solace (0-99)");
      Serial.println("  stop             - Stop all movement");
      Serial.println();
      Serial.println("Debug Commands:");
      Serial.println("  scanall          - Scan ALL BLE devices (not just Kiiroo)");
      Serial.println("  scanlong         - Extra long scan (30 seconds)");
      Serial.println("  details          - Show detailed device info");
      Serial.println("  services <addr>  - Show services for specific device");
      Serial.println("  rssi             - Show signal strength of found devices");
      Serial.println("  bleinfo          - Show BLE adapter information");
      Serial.println("  reset            - Reset BLE adapter");
      Serial.println("  test             - Test basic BLE functionality");
      Serial.println("  advertise        - Start advertising (make ESP32 visible)");
      Serial.println("  stopadv          - Stop advertising");
      Serial.println("  verbose          - Toggle verbose logging");
      Serial.println("=========================================");
    }
    
    else if (command == "scan") {
      Serial.println("[CMD] Scanning for Kiiroo devices...");
      if (scanForKiirooDevices()) {
        Serial.printf("[CMD] Found %d Kiiroo device(s)\n", discoveredDevices.size());
        for (int i = 0; i < discoveredDevices.size(); i++) {
          auto& dev = discoveredDevices[i];
          Serial.printf("  %d: %s (%s)\n", i+1, dev.name.c_str(), dev.address.toString().c_str());
        }
      } else {
        Serial.println("[CMD] No Kiiroo devices found");
      }
    }
    
    else if (command == "keon") {
      Serial.println("[CMD] Attempting Keon connection...");
      if (connectToKeonDevice()) {
        keonConnected = true;
        Serial.println("[CMD] Keon connected!");
      } else {
        Serial.println("[CMD] Keon connection failed!");
      }
    }
    
    else if (command == "solace") {
      Serial.println("[CMD] Attempting Solace connection...");
      if (connectToSolaceDevice()) {
        solaceConnected = true;
        Serial.println("[CMD] Solace connected!");
      } else {
        Serial.println("[CMD] Solace connection failed!");
      }
    }
    
    else if (command == "disconnect") {
      Serial.println("[CMD] Disconnecting all devices...");
      if (keonConnected) {
        disconnectKeonDevice();
        Serial.println("[CMD] Keon disconnected");
      }
      if (solaceConnected) {
        disconnectSolaceDevice();
        Serial.println("[CMD] Solace disconnected");
      }
    }
    
    else if (command == "status") {
      Serial.println("\n=== STATUS ===");
      Serial.printf("Keon: %s\n", keonConnected ? "CONNECTED" : "OFF");
      Serial.printf("Solace: %s\n", solaceConnected ? "CONNECTED" : "OFF");
      Serial.printf("Nunchuk: %s\n", nunchukReady ? "OK" : "FAIL");
      Serial.printf("Position: %d\n", currentKeonPosition);
      Serial.printf("Speed: %d\n", currentKeonSpeed);
      Serial.println("==============");
    }
    
    else if (command.startsWith("move ")) {
      int pos, spd;
      if (sscanf(command.c_str(), "move %d %d", &pos, &spd) == 2) {
        pos = constrain(pos, 0, 99);
        spd = constrain(spd, 0, 99);
        Serial.printf("[CMD] Move: pos=%d speed=%d\n", pos, spd);
        if (keonConnected) {
          sendKeonCommand(pos, spd);
        } else {
          Serial.println("[CMD] Keon not connected!");
        }
        currentKeonPosition = pos;
        currentKeonSpeed = spd;
      }
    }
    
    else if (command.startsWith("solace ")) {
      int val = command.substring(7).toInt();
      val = constrain(val, 0, 99);
      Serial.printf("[CMD] Solace command: value=%d\n", val);
      if (solaceConnected) {
        sendSolaceCommand(val);
      } else {
        Serial.println("[CMD] Solace not connected!");
      }
    }
    
    else if (command == "stop") {
      Serial.println("[CMD] Stopping all devices");
      if (keonConnected) {
        sendKeonCommand(currentKeonPosition, 0);
        currentKeonSpeed = 0;
        Serial.println("[CMD] Keon stopped");
      }
      if (solaceConnected) {
        sendSolaceCommand(0);
        Serial.println("[CMD] Solace stopped");
      }
    }
    
    // ====== DEBUG COMMANDS ======
    
    else if (command == "scanall") {
      Serial.println("[CMD] Scanning ALL BLE devices (not filtered)...");
      scanAllDevices();
    }
    
    else if (command == "scanlong") {
      Serial.println("[CMD] Extra long scan (30 seconds)...");
      scanForKiirooDevicesLong(30);
    }
    
    else if (command == "details") {
      Serial.println("[CMD] Detailed device information:");
      showDetailedDeviceInfo();
    }
    
    else if (command.startsWith("services ")) {
      String addrStr = command.substring(9);
      addrStr.trim();
      Serial.printf("[CMD] Scanning services for device %s...\n", addrStr.c_str());
      scanDeviceServices(addrStr);
    }
    
    else if (command == "rssi") {
      Serial.println("[CMD] Signal strength information:");
      showRSSIInfo();
    }
    
    else if (command == "bleinfo") {
      Serial.println("[CMD] BLE adapter information:");
      showBLEInfo();
    }
    
    else if (command == "reset") {
      Serial.println("[CMD] Resetting BLE adapter...");
      resetBLEAdapter();
    }
    
    else if (command == "test") {
      Serial.println("[CMD] Testing basic BLE functionality...");
      testBLEBasics();
    }
    
    else if (command == "advertise") {
      Serial.println("[CMD] Starting BLE advertising...");
      startAdvertising();
    }
    
    else if (command == "stopadv") {
      Serial.println("[CMD] Stopping BLE advertising...");
      stopAdvertising();
    }
    
    else if (command == "verbose") {
      verboseLogging = !verboseLogging;
      Serial.printf("[CMD] Verbose logging: %s\n", verboseLogging ? "ON" : "OFF");
    }
  }
}

