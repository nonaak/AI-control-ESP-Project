// ═══════════════════════════════════════════════════════════════════════════
// HOOFT_ESP_KEON.INO - COMPLETE VERSIE MET ESP-NOW + KEON
// ═══════════════════════════════════════════════════════════════════════════
// Deze versie heeft:
// ✅ ESP-NOW enabled (Body ESP, Pump, M5Atom)
// ✅ Keon BLE control (keonInit, keonCheckConnection, keonIndependentTick)
// ✅ Correcte volgorde in loop() voor minimaal conflict
// ═══════════════════════════════════════════════════════════════════════════

#include "ui.h"
#include "espnow_comm.h"
#include "keon_ble.h"

void setup() {
  // ═══════════════════════════════════════════════════════════════════════
  // INITIALIZATION SEQUENCE
  // ═══════════════════════════════════════════════════════════════════════
  
  // 1. Initialize UI first (display, settings, menu)
  uiInit();
  
  // 2. Initialize Keon BLE controller
  Serial.println("[SYSTEM] Initializing Keon BLE...");
  keonInit();
  
  // 3. Initialize ESP-NOW communication (Body ESP, Pump, M5Atom)
  Serial.println("[SYSTEM] Initializing ESP-NOW...");
  initESPNow();

  keonStartTask();  // ✅ START CORE 0 TASK
  
  // ═══════════════════════════════════════════════════════════════════════
  // STARTUP BANNER
  // ═══════════════════════════════════════════════════════════════════════
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║  HoofdESP - Full System Online            ║");
  Serial.println("║  ESP-NOW + Keon BLE Combined Mode          ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println("[SYSTEM] MAC: E4:65:B8:7A:85:E4 - Channel: 4");
  Serial.println("[SYSTEM] Keon BLE ready for connection");
  Serial.println("[SYSTEM] ESP-NOW communication active");
  Serial.println("[SYSTEM] Body ESP, Pump Unit, M5Atom online");
  Serial.println("[SYSTEM] Motion sync enabled");
}

void loop() {
  // ═══════════════════════════════════════════════════════════════════════
  // MAIN LOOP - OPTIMIZED ORDER FOR MINIMAL RADIO CONFLICT
  // ═══════════════════════════════════════════════════════════════════════
  
  // ───────────────────────────────────────────────────────────────────────
  // 1. UI HANDLING (menu, animation, input)
  // ───────────────────────────────────────────────────────────────────────
  uiTick();
  
  // ───────────────────────────────────────────────────────────────────────
  // 2. KEON BLE HANDLING (autonomous control)
  // ───────────────────────────────────────────────────────────────────────
  // BELANGRIJK: Dit zijn de functies die Keon laten werken!
  // - keonCheckConnection(): Controleert of verbinding nog actief is
  // - keonIndependentTick(): Autonome Keon beweging control
  //   * Checkt 'paused' flag zelf
  //   * Handled parking bij pause
  //   * Geen interference van UI!
  //keonCheckConnection();
  //keonIndependentTick();
  
  // ───────────────────────────────────────────────────────────────────────
  // 3. ESP-NOW COMMUNICATION (Body ESP, Pump, M5Atom)
  // ───────────────────────────────────────────────────────────────────────
  
  checkCommunicationTimeouts();
  updateVacuumControl();
  sendPumpControlMessages();
  sendStatusUpdates();
  performSafetyChecks();
  

  // ───────────────────────────────────────────────────────────────────────
  // 4. MOTION SYNC (M5Atom motion controls animation speed)
  // ───────────────────────────────────────────────────────────────────────
  syncMotionToAnimation();
  
  // ───────────────────────────────────────────────────────────────────────
  // 5. SYSTEM TIMING (prevent overwhelming)
  // ───────────────────────────────────────────────────────────────────────
  delay(1);
}

// ═══════════════════════════════════════════════════════════════════════════
// NOTES:
// ═══════════════════════════════════════════════════════════════════════════
//
// RADIO CONFLICT MINIMIZATION:
// - Keon BLE en ESP-NOW delen dezelfde 2.4GHz radio
// - Volgorde is belangrijk: UI → Keon → ESP-NOW
// - Als Keon ruis heeft met ESP-NOW enabled:
//   * Verhoog levelIntervals[] in keon_ble.cpp
//   * Overweeg WiFi channel switch (nu channel 4)
//   * Test verschillende channels (1, 6, 11)
//
// KEON AUTONOMY:
// - keonIndependentTick() controleert 'paused' flag zelf
// - UI.CPP mag GEEN keonParkToBottom() meer aanroepen!
// - Keon handled pause/resume volledig autonoom
//
// ESP-NOW DEVICES:
// - Body ESP: Biometric sensors (HR, GSR, etc)
// - Pump Unit: Vacuum control
// - M5Atom: Motion sync voor animation speed
//
// ═══════════════════════════════════════════════════════════════════════════
