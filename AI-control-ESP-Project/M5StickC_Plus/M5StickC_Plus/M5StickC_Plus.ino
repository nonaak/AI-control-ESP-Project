/*
 * === M5StickC Plus — Motion TX + Pump Colors RX (ESP-NOW, CH=4) ===
 * Met SERIEEL MENU voor gevoeligheid & gedrag.
 * - Up    => neon paarse pijl omhoog op LCD display
 * - Down  => rode pijl omlaag op LCD display  
 * - Stil  => donkerrode horizontale lijn op LCD display
 * - IMU → dir/speed; TX naar head; RX kleuren van head; ALTIJD tekenen (~30 FPS)
 * Core: Arduino-ESP32 v3.x (IDF5) — nieuwe esp-now callbacks
 * 
 * Hardware: M5StickC Plus (SKU:K016-P)
 * Display: ST7789V2 135x240 LCD
 * IMU: MPU6886
 */

#include <M5StickCPlus.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

// =============================================================================
// ⚡ GEVOELIGHEID INSTELLING - Verander dit getal van 1 tot 100
// =============================================================================
// Lagere waarde = GEVOELIGER (reageert sneller op kleine bewegingen)
// Hogere waarde = MINDER GEVOELIG (vereist grotere bewegingen)
// Aanbevolen: 30-70 voor normale gebruik, 10-30 voor zeer gevoelig
// DEBUGGING: Tijdelijk zeer gevoelig voor testen
static const int SENSITIVITY = 35;  // ← VERANDER DIT GETAL (1-100)
// =============================================================================

// =============================================================================
// 🏠 RUST TRANSITIE TIJD - Verander dit getal van 300 tot 2000 (milliseconden)
// =============================================================================
// Tijd tussen stilstand detectie → pijl beneden → rode horizontale lijn
// Lagere waarde = SNELLERE transitie naar rode lijn
// Hogere waarde = LANGERE tijd met pijl beneden voordat rode lijn verschijnt
// Aanbevolen: 500-1200ms voor normale gebruik
static const int REST_DELAY = 800;  // ← VERANDER DIT GETAL (300-2000) milliseconden
// =============================================================================

// ---- Peer / kanaal ----
static const char* HEAD_MAC_STR = "E4:65:B8:7A:85:E4";   // Hoofd-ESP MAC adres
static const uint8_t WIFI_CHANNEL = 4;                   // vast kanaal 4 (belangrijk)

// ---- LED Hardware Configuration ----
#define LED_VACUUM_PIN 21    // GPIO 21 - Vacuum pump status LED
#define LED_MANUAL_PIN 22    // GPIO 22 - Manual toggle LED

// ---- LED Global Variables ----
volatile bool g_led_vacuum_state = false;     // Current vacuum LED state
volatile bool g_led_manual_state = false;     // Current manual LED state
volatile bool g_led_vacuum_cmd = false;       // Received vacuum LED command from HoofdESP
volatile bool g_led_manual_cmd = false;       // Received manual LED command from HoofdESP
volatile bool g_led_manual_local_toggle = false; // Local manual toggle state

// ---- Communication timeout for safety ----
volatile uint32_t g_last_heartbeat = 0;       // Last received message timestamp
static const uint32_t HEARTBEAT_TIMEOUT = 5000; // 5 seconds timeout
volatile bool g_connection_active = false;    // Connection status

// ---- Protocol ----
#define MSG_VER 1
enum MsgKind : uint8_t { MK_ATOM_MOTION = 1, MK_PUMP_COLORS = 2 };

// Original protocol structures (compatible with HoofdESP)
struct AtomMotion { 
  uint8_t ver, kind; 
  int8_t dir; 
  uint8_t speed; 
  uint8_t flags;  // bits 0-1: original, bits 2-3: LED status
};

struct PumpColors { 
  uint8_t ver, kind; 
  uint8_t a_r, a_g, a_b, b_r, b_g, b_b; 
  uint8_t flags;  // bits 0-1: pumps, bits 2-3: LED commands
};

// ---- Display/orientatie ----
enum Orientation { USB_UP=0, USB_RIGHT=1, USB_DOWN=2, USB_LEFT=3 };
static Orientation ORIENT = USB_UP;

// Display constants
const int SCREEN_W = 135;
const int SCREEN_H = 240;
const int ARROW_SIZE = 120;  // Much bigger arrow size

// RGB565 color conversion
uint16_t rgb888to565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ---- LED Control Functions ----
void initLEDs() {
  pinMode(LED_VACUUM_PIN, OUTPUT);
  pinMode(LED_MANUAL_PIN, OUTPUT);
  digitalWrite(LED_VACUUM_PIN, LOW);
  digitalWrite(LED_MANUAL_PIN, LOW);
  Serial.println("LEDs initialized (GPIO 21: Vacuum, GPIO 22: Manual)");
}

void setLED(uint8_t pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

void updateLEDs() {
  // Check connection timeout for safety
  uint32_t now = millis();
  if (g_connection_active && (now - g_last_heartbeat > HEARTBEAT_TIMEOUT)) {
    g_connection_active = false;
    g_led_vacuum_cmd = false;
    g_led_manual_cmd = false;
    Serial.println("⚠️ Connection timeout - LEDs disabled for safety");
  }
  
  // Update vacuum LED based on HoofdESP command (if connected)
  bool vacuum_target = g_connection_active ? g_led_vacuum_cmd : false;
  if (g_led_vacuum_state != vacuum_target) {
    g_led_vacuum_state = vacuum_target;
    setLED(LED_VACUUM_PIN, g_led_vacuum_state);
    Serial.printf("💡 Vacuum LED %s\n", g_led_vacuum_state ? "ON" : "OFF");
  }
  
  // Update manual LED: local toggle OR HoofdESP command
  bool manual_target = g_led_manual_local_toggle || (g_connection_active ? g_led_manual_cmd : false);
  if (g_led_manual_state != manual_target) {
    g_led_manual_state = manual_target;
    setLED(LED_MANUAL_PIN, g_led_manual_state);
    Serial.printf("💡 Manual LED %s (Local:%s, Remote:%s)\n", 
                  g_led_manual_state ? "ON" : "OFF",
                  g_led_manual_local_toggle ? "ON" : "OFF",
                  g_led_manual_cmd ? "ON" : "OFF");
  }
}

void testLEDs() {
  Serial.println("🔧 Testing LEDs...");
  
  // Test Vacuum LED
  setLED(LED_VACUUM_PIN, HIGH);
  Serial.println("Vacuum LED ON");
  delay(500);
  setLED(LED_VACUUM_PIN, LOW);
  Serial.println("Vacuum LED OFF");
  delay(300);
  
  // Test Manual LED
  setLED(LED_MANUAL_PIN, HIGH);
  Serial.println("Manual LED ON");
  delay(500);
  setLED(LED_MANUAL_PIN, LOW);
  Serial.println("Manual LED OFF");
  
  Serial.println("✅ LED test complete");
}

// Colors
const uint16_t COL_BLACK = TFT_BLACK;
const uint16_t COL_RED = TFT_RED;
const uint16_t COL_WHITE = TFT_WHITE;
const uint16_t COL_GREEN = TFT_GREEN;
const uint16_t COL_BLUE = TFT_BLUE;
const uint16_t COL_NEON_PURPLE = rgb888to565(255, 0, 255);  // Neon paars
const uint16_t COL_DARK_RED = rgb888to565(139, 0, 0);      // Donker rood

// ---- Display Functions ----
void clearDisplay() {
  M5.Lcd.fillScreen(COL_BLACK);
}

void drawArrowUp(uint16_t color, bool clear = true) {
  if (clear) clearDisplay();
  
  // Draw SEXY UP arrow in center of screen
  int centerX = SCREEN_W / 2;
  int centerY = SCREEN_H / 2;
  
  // Arrow dimensions
  int shaftWidth = 24;
  int shaftHeight = 70;
  int headWidth = 50;
  int headHeight = 40;
  
  // Position calculations - head ABOVE shaft
  int headTop = centerY - shaftHeight/2 - headHeight;
  int shaftTop = centerY - shaftHeight/2;
  
  // Draw arrow head FIRST (triangle pointing up)
  for (int i = 0; i < headHeight; i++) {
    int lineWidth = (headWidth * (i + 1)) / headHeight;  // Start narrow, get wider
    int lineStart = centerX - lineWidth/2;
    int lineEnd = centerX + lineWidth/2;
    M5.Lcd.drawLine(lineStart, headTop + i, lineEnd, headTop + i, color);
  }
  
  // Draw arrow shaft BELOW head (no overlap)
  M5.Lcd.fillRoundRect(centerX - shaftWidth/2, shaftTop, 
                       shaftWidth, shaftHeight, 6, color);
  
  // Clean arrow - no glow effects
}

void drawArrowDown(uint16_t color, bool clear = true) {
  if (clear) clearDisplay();
  
  // Draw SEXY DOWN arrow in center of screen
  int centerX = SCREEN_W / 2;
  int centerY = SCREEN_H / 2;
  
  // Arrow dimensions
  int shaftWidth = 24;
  int shaftHeight = 70;
  int headWidth = 50;
  int headHeight = 40;
  
  // Position calculations - head BELOW shaft
  int shaftTop = centerY - shaftHeight/2;
  int headTop = centerY + shaftHeight/2;
  
  // Draw arrow shaft FIRST (above head)
  M5.Lcd.fillRoundRect(centerX - shaftWidth/2, shaftTop, 
                       shaftWidth, shaftHeight, 6, color);
  
  // Draw arrow head BELOW shaft (triangle pointing down)
  for (int i = 0; i < headHeight; i++) {
    int lineWidth = (headWidth * (headHeight - i)) / headHeight;
    int lineStart = centerX - lineWidth/2;
    int lineEnd = centerX + lineWidth/2;
    M5.Lcd.drawLine(lineStart, headTop + i, lineEnd, headTop + i, color);
  }
  
  // Clean arrow - no glow effects
}

void drawHorizontalLine(uint16_t color, bool clear = true) {
  if (clear) clearDisplay();
  
  // Draw SEXY horizontal line in center of screen
  int centerY = SCREEN_H / 2;
  int lineHeight = 16;  // Much thicker line
  int margin = 15;
  
  // Main thick line with rounded edges
  M5.Lcd.fillRoundRect(margin, centerY - lineHeight/2, 
                       SCREEN_W - 2*margin, lineHeight, 8, color);
  
  // Add glow effect for dark red
  if (color == COL_DARK_RED) {
    uint16_t glowColor = rgb888to565(69, 0, 0);  // Even dimmer dark red
    M5.Lcd.drawRoundRect(margin - 2, centerY - lineHeight/2 - 2, 
                         SCREEN_W - 2*margin + 4, lineHeight + 4, 10, glowColor);
  }
}

// ---- ESP-NOW peer naar head ----
uint8_t headMac[6]; 
esp_now_peer_info_t headPeer{};

// ---- RX pompstatus ----
volatile uint8_t g_a_r=0,g_a_g=0,g_a_b=0;
volatile uint8_t g_b_r=0,g_b_g=0,g_b_b=0;
volatile uint8_t g_flags=0;
volatile bool    g_colorsUpdated=false;

// ---- IMU state ----
float g_est_x=0, g_est_y=0, g_est_z=1.0f; // gravity low-pass
float v_up = 0.0f;
int8_t motion_dir = 0;                    // start "stilstand" -> direct horizontale streep zichtbaar
uint8_t motion_speed = 0;

// ---- Stilstand transitie state ----
uint32_t g_stillStartTime = 0;            // wanneer we stilstand detecteerden
bool g_inRestTransition = false;          // zijn we in de transitie naar rust?

// ---- Tuning (runtime via menu) ----
struct Config {
  float    lpfAlpha     = 0.985f;  // gravity low-pass (0.98..0.995 typ.)
  float    damp         = 0.992f;  // demping van v_up    (0.985..0.998)
  float    accEps       = 0.015f;  // directe richtingsdrempel op |lin_up| in g (0.010..0.030)
  float    speedGain    = 260.0f;  // schaal → 0..100 (100..400)
  uint16_t sendInterval = 40;      // ms, TX naar head (20..120)
  bool     flipDir      = false;   // invert up/down
  uint8_t  drawInterval = 33;      // ms, ~30 FPS
  uint16_t restDelay    = 800;     // ms, tijd van pijl beneden naar rode lijn bij stilstand (300..2000)
  Orientation orient    = USB_UP;  // USB_UP/RIGHT/DOWN/LEFT
} cfg;

// ---- Automatische gevoeligheid configuratie (1-100) ----
void applySensitivity(int sens) {
  // Beperk tot 1-100 bereik
  sens = max(1, min(100, sens));
  
  // Converteer 1-100 naar optimale waarden:
  // SENSITIVITY 1 = zeer gevoelig, 100 = weinig gevoelig
  
  // accEps: Directe drempel (lager = gevoeliger)
  // Bereik: 0.005 (zeer gevoelig) tot 0.050 (weinig gevoelig)
  cfg.accEps = 0.005f + (sens - 1) * 0.045f / 99.0f;
  
  // speedGain: Snelheidsversterking (hoger = gevoeliger voor snelheid)
  // Bereik: 400 (zeer gevoelig) tot 120 (weinig gevoelig)
  cfg.speedGain = 400.0f - (sens - 1) * 280.0f / 99.0f;
  
  // damp: Demping (lager = responsiever, hoger = stabieler)
  // Bereik: 0.985 (responsief) tot 0.998 (stabiel)
  cfg.damp = 0.985f + (sens - 1) * 0.013f / 99.0f;
  
  // lpfAlpha: Low-pass filter (lager = responsiever)
  // Bereik: 0.980 (responsief) tot 0.995 (stabiel)
  cfg.lpfAlpha = 0.980f + (sens - 1) * 0.015f / 99.0f;
  
  Serial.printf("\n⚡ GEVOELIGHEID INGESTELD OP %d/100\n", sens);
  Serial.printf("   accEps=%.4f, speedGain=%.1f, damp=%.3f, lpfAlpha=%.3f\n",
                cfg.accEps, cfg.speedGain, cfg.damp, cfg.lpfAlpha);
}

// ---- Kanaal forceren ----
void forceWifiChannel(uint8_t ch){
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

// ---- TX naar head ----
void sendMotion(){ 
  // Pack LED status in flags: bit 2=vacuum, bit 3=manual
  uint8_t flags = 0;
  if (g_led_vacuum_state) flags |= 0x04;  // bit 2
  if (g_led_manual_state) flags |= 0x08;  // bit 3
  
  AtomMotion m{MSG_VER, MK_ATOM_MOTION, motion_dir, motion_speed, flags}; 
  esp_now_send(headPeer.peer_addr, (uint8_t*)&m, sizeof(m)); 
  
  // Debug output - alleen bij beweging of snelheid > 0
  if (motion_dir != 0 || motion_speed > 0) {
    const char* dirStr = (motion_dir > 0) ? "UP" : (motion_dir < 0) ? "DOWN" : "STIL";
    Serial.printf("🚀 TX -> Dir:%s(%+d) Speed:%u LEDs(V:%u,M:%u)\n", 
                  dirStr, motion_dir, motion_speed,
                  g_led_vacuum_state ? 1u : 0u, g_led_manual_state ? 1u : 0u);
  }
}

// ---- RX callback (nieuwe signatuur IDF5) ----
void onDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len){
  if (len < (int)sizeof(PumpColors)) return;
  const PumpColors* c = (const PumpColors*)data;
  if (c->ver!=MSG_VER || c->kind!=MK_PUMP_COLORS) return;
  
  // Update pump colors (original functionality)
  g_a_r=c->a_r; g_a_g=c->a_g; g_a_b=c->a_b;
  g_b_r=c->b_r; g_b_g=c->b_g; g_b_b=c->b_b;
  g_flags=c->flags; g_colorsUpdated=true;
  
  // Extract LED commands from flags: bit 2=vacuum, bit 3=manual
  g_led_vacuum_cmd = (c->flags & 0x04) != 0;
  g_led_manual_cmd = (c->flags & 0x08) != 0;
  
  // Update connection heartbeat
  g_last_heartbeat = millis();
  g_connection_active = true;
  
  // Debug LED commands received
  static uint32_t lastLEDDebug = 0;
  if (millis() - lastLEDDebug > 2000) {  // Debug every 2 seconds
    lastLEDDebug = millis();
    Serial.printf("📡 RX LED cmds: flags=0x%02X Vacuum=%u, Manual=%u\n", 
                  c->flags, g_led_vacuum_cmd ? 1u : 0u, g_led_manual_cmd ? 1u : 0u);
  }
}

// ---- LED Status Display Functions ----
void drawLEDStatus() {
  // Draw LED status indicators in top corners
  const int indicator_size = 8;
  const int margin = 5;
  
  // Vacuum LED indicator (top-left)
  uint16_t vacuum_color = g_led_vacuum_state ? COL_GREEN : COL_DARK_RED;
  M5.Lcd.fillCircle(margin + indicator_size/2, margin + indicator_size/2, indicator_size/2, vacuum_color);
  
  // Manual LED indicator (top-right)  
  uint16_t manual_color = g_led_manual_state ? COL_BLUE : COL_DARK_RED;
  M5.Lcd.fillCircle(SCREEN_W - margin - indicator_size/2, margin + indicator_size/2, indicator_size/2, manual_color);
  
  // Connection status indicator (top-center)
  uint16_t conn_color = g_connection_active ? COL_GREEN : COL_RED;
  M5.Lcd.fillRect(SCREEN_W/2 - 3, margin, 6, 3, conn_color);
}

// ---- Tekenen (ALTIJD, ~30 FPS) ----
void drawFromState(){
  bool A_on = (g_flags & 0x01) || (g_a_r|g_a_g|g_a_b);
  bool B_on = (g_flags & 0x02) || (g_b_r|g_b_g|g_b_b);
  
  if (motion_dir > 0) { 
    // UP beweging: neon paarse pijl omhoog
    drawArrowUp(COL_NEON_PURPLE, true); 
  }
  else if (motion_dir < 0) {
    // DOWN beweging: rode pijl omlaag
    drawArrowDown(COL_RED, true);
  }
  else {
    // STILSTAND (motion_dir == 0): check transitie fase
    if (g_inRestTransition) {
      uint32_t timeInRest = millis() - g_stillStartTime;
      if (timeInRest < cfg.restDelay) {
        // Nog in transitie: toon rode pijl naar beneden (ruststand)
        drawArrowDown(COL_RED, true);
      } else {
        // Transitie tijd voorbij: toon horizontale donkerrode lijn
        drawHorizontalLine(COL_DARK_RED, true);
      }
    } else {
      // Niet in transitie: direct horizontale donkerrode lijn
      drawHorizontalLine(COL_DARK_RED, true);
    }
  }
  
  // Add LED status indicators over the main display
  drawLEDStatus();
}

// ---- IMU update ----
void updateMotion(float dt){
  float ax, ay, az; 
  M5.IMU.getAccelData(&ax, &ay, &az); // 'g'
  
  // DEBUG: Print raw accelerometer data elke 1 seconde
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();
    Serial.printf("[IMU DEBUG] Raw: ax=%.3f, ay=%.3f, az=%.3f\n", ax, ay, az);
    Serial.printf("[IMU DEBUG] Gravity est: gx=%.3f, gy=%.3f, gz=%.3f\n", g_est_x, g_est_y, g_est_z);
  }

  // gravity low-pass
  g_est_x = cfg.lpfAlpha*g_est_x + (1.0f-cfg.lpfAlpha)*ax;
  g_est_y = cfg.lpfAlpha*g_est_y + (1.0f-cfg.lpfAlpha)*ay;
  g_est_z = cfg.lpfAlpha*g_est_z + (1.0f-cfg.lpfAlpha)*az;

  // up-unit = -normalized(gravity)
  float glen = sqrtf(g_est_x*g_est_x + g_est_y*g_est_y + g_est_z*g_est_z);
  float ux=0, uy=0, uz=1; 
  if (glen>1e-3f){ 
    ux=-g_est_x/glen; 
    uy=-g_est_y/glen; 
    uz=-g_est_z/glen; 
  }

  // lineaire a (g), projecteer op up-as
  float lin_x = ax - g_est_x, lin_y = ay - g_est_y, lin_z = az - g_est_z;
  float lin_up = lin_x*ux + lin_y*uy + lin_z*uz;
  if (cfg.flipDir) lin_up = -lin_up;

  // Bewaar vorige motion_dir om state changes te detecteren
  int8_t prev_motion_dir = motion_dir;
  
  // DEBUG: Print calculation values
  if (millis() - lastDebug < 50) {  // Same debug cycle as above
    Serial.printf("[MOTION DEBUG] lin_up=%.4f, v_up=%.4f, accEps=%.4f\n", lin_up, v_up, cfg.accEps);
    Serial.printf("[MOTION DEBUG] up_vector: ux=%.3f, uy=%.3f, uz=%.3f\n", ux, uy, uz);
  }
  
  // Update v_up ALTIJD (voor speed berekening)
  v_up = v_up*cfg.damp + lin_up*dt;   // integreer licht gedempt
  
  // DIRECTE richting bij |lin_up| > accEps, anders op basis van v_up
  if (lin_up >  cfg.accEps)      motion_dir = +1;
  else if (lin_up < -cfg.accEps) motion_dir = -1;
  else {
    // Verbeterde stilstand detectie met drempel
    float stillThreshold = 0.02f;  // Drempel voor stilstand (aanpasbaar)
    
    if      (v_up >  stillThreshold) motion_dir = +1;
    else if (v_up < -stillThreshold) motion_dir = -1;
    else                             motion_dir = 0;  // STILSTAND
  }
  
  // 🏠 RUST TRANSITIE LOGICA
  uint32_t currentTime = millis();
  
  // Check of we net in stilstand zijn gekomen
  if (motion_dir == 0 && prev_motion_dir != 0) {
    // We gaan net in stilstand - start rust transitie
    g_stillStartTime = currentTime;
    g_inRestTransition = true;
    Serial.println("⬇️ Stilstand gedetecteerd - pijl naar beneden (ruststand)");
  }
  
  // Check of we weer beweging hebben tijdens transitie
  if (g_inRestTransition && motion_dir != 0) {
    // Beweging gedetecteerd - stop transitie
    g_inRestTransition = false;
    Serial.println("🔄 Beweging hervat - transitie gestopt");
  }

  // snelheid (0..100) — licht gesmoothd
  static float sp_lp = 0.0f;
  float sp_now = fabsf(v_up) * cfg.speedGain; 
  if (sp_now>100.f) sp_now=100.f;
  sp_lp = 0.7f*sp_lp + 0.3f*sp_now;
  motion_speed = (uint8_t)(sp_lp + 0.5f);
}

// ---- Serieel menu ----
String rxLine;
void printHelp(){
  Serial.println(F("\n=== M5StickC Plus Motion + LED Control Menu (115200) ==="));
  Serial.println(F("H or ?           : help"));
  Serial.println(F("S                : show current settings"));
  Serial.println(F("⚡ SENS <1..100>   : Eenvoudige gevoeligheid (1=zeer gevoelig, 100=weinig gevoelig)"));
  Serial.println(F("EPS <f>          : set accEps (g), e.g. 0.012 .. 0.030 (lower = sneller omslaan)"));
  Serial.println(F("GAIN <f>         : set speedGain, e.g. 150..400 (higher = gevoeliger)"));
  Serial.println(F("LPF <f>          : set lpfAlpha 0.98..0.995 (hoger = stabieler, langzamer)"));
  Serial.println(F("DAMP <f>         : set damping 0.985..0.998 (hoger = trager, minder jitter)"));
  Serial.println(F("SEND <ms>        : TX interval naar head (10..200 ms)"));
  Serial.println(F("REST <ms>        : Rust transitie tijd (300..2000 ms) - van pijl beneden naar rode lijn"));
  Serial.println(F("FLIP <0|1>       : invert up/down detectie"));
  Serial.println(F("ORIENT <UP|RIGHT|DOWN|LEFT>  of  ORIENT <0..3>"));
  Serial.println(F("\n💡 LED CONTROL:"));
  Serial.println(F("Button A         : Toggle manual LED (GPIO 22)"));
  Serial.println(F("GPIO 21          : Vacuum pump LED (controlled by HoofdESP)"));
  Serial.println(F("GPIO 22          : Manual LED (local toggle + HoofdESP)"));
  Serial.println(F("Display          : Green=ON, Dark Red=OFF, Blue=Manual LED"));
  Serial.println(F("\nVoorbeeld:  SENS 30   (gevoelig)  of  SENS 70   (stabiel)"));
  Serial.println(F("Advanced:   EPS 0.012   GAIN 300   LPF 0.99   DAMP 0.995   REST 800"));
}

void printConfig(){
  Serial.printf("EPS=%.4f  GAIN=%.1f  LPF=%.3f  DAMP=%.3f  SEND=%ums  REST=%ums  FLIP=%u  ORIENT=%d\n",
    cfg.accEps, cfg.speedGain, cfg.lpfAlpha, cfg.damp, (unsigned)cfg.sendInterval, (unsigned)cfg.restDelay,
    (unsigned)cfg.flipDir, (int)cfg.orient);
}

Orientation parseOrient(const String& t){
  if (t=="UP") return USB_UP; 
  if (t=="RIGHT") return USB_RIGHT;
  if (t=="DOWN") return USB_DOWN; 
  if (t=="LEFT") return USB_LEFT;
  return ORIENT;
}

void handleLine(String ln){
  ln.trim(); 
  if (!ln.length()) return;
  String s = ln; 
  s.toUpperCase();
  if (s=="H" || s=="?") { printHelp(); return; }
  if (s=="S") { printConfig(); return; }

  if (s.startsWith("SENS ")) { 
    int v = s.substring(5).toInt(); 
    if (v < 1) v = 1; 
    if (v > 100) v = 100; 
    applySensitivity(v); 
    return; 
  }
  if (s.startsWith("EPS ")) { 
    float v=s.substring(4).toFloat(); 
    if (v<0.001f) v=0.001f; 
    if (v>0.1f) v=0.1f; 
    cfg.accEps=v; 
    Serial.printf("EPS -> %.4f g\n", cfg.accEps); 
    return; 
  }
  if (s.startsWith("GAIN ")) { 
    float v=s.substring(5).toFloat(); 
    if (v<10.f) v=10.f; 
    if (v>1000.f) v=1000.f; 
    cfg.speedGain=v; 
    Serial.printf("GAIN -> %.1f\n", cfg.speedGain); 
    return; 
  }
  if (s.startsWith("LPF ")) { 
    float v=s.substring(4).toFloat(); 
    if (v<0.90f) v=0.90f; 
    if (v>0.999f) v=0.999f; 
    cfg.lpfAlpha=v; 
    Serial.printf("LPF -> %.3f\n", cfg.lpfAlpha); 
    return; 
  }
  if (s.startsWith("DAMP ")) { 
    float v=s.substring(5).toFloat(); 
    if (v<0.90f) v=0.90f; 
    if (v>0.999f) v=0.999f; 
    cfg.damp=v; 
    Serial.printf("DAMP -> %.3f\n", cfg.damp); 
    return; 
  }
  if (s.startsWith("SEND ")) { 
    int v=s.substring(5).toInt(); 
    if (v<10) v=10; 
    if (v>500) v=500; 
    cfg.sendInterval=(uint16_t)v; 
    Serial.printf("SEND -> %u ms\n", (unsigned)cfg.sendInterval); 
    return; 
  }
  if (s.startsWith("REST ")) { 
    int v=s.substring(5).toInt(); 
    if (v<300) v=300; 
    if (v>2000) v=2000; 
    cfg.restDelay=(uint16_t)v; 
    Serial.printf("REST -> %u ms\n", (unsigned)cfg.restDelay); 
    return; 
  }
  if (s.startsWith("FLIP ")) { 
    int v=s.substring(5).toInt(); 
    cfg.flipDir=(v!=0); 
    Serial.printf("FLIP -> %u\n", (unsigned)cfg.flipDir); 
    return; 
  }
  if (s.startsWith("ORIENT ")) {
    String t = s.substring(7); 
    t.trim();
    Orientation o = ORIENT;
    if (t=="0"||t=="UP") o=USB_UP; 
    else if (t=="1"||t=="RIGHT") o=USB_RIGHT;
    else if (t=="2"||t=="DOWN") o=USB_DOWN; 
    else if (t=="3"||t=="LEFT") o=USB_LEFT;
    cfg.orient = o; 
    ORIENT = o; 
    Serial.printf("ORIENT -> %d\n",(int)ORIENT); 
    return;
  }

  Serial.println(F("Onbekend commando. Type 'H' voor help."));
}

void pollSerialMenu(){
  while (Serial.available()){
    char c = Serial.read(); 
    if (c=='\r') continue;
    if (c=='\n'){ 
      if (rxLine.length()){ 
        handleLine(rxLine); 
        rxLine=""; 
      } 
    }
    else { 
      rxLine += c; 
      if (rxLine.length()>80) rxLine.remove(0); 
    }
  }
}

// ---- Button handling for manual LED toggle ----
void handleButtons() {
  static uint32_t btnA_press_start = 0;
  static bool btnA_was_pressed = false;
  static uint32_t last_toggle = 0;
  
  // Check button A state
  bool btnA_pressed = M5.BtnA.isPressed();
  
  // Handle button A press/release
  if (btnA_pressed && !btnA_was_pressed) {
    // Button just pressed
    btnA_press_start = millis();
    btnA_was_pressed = true;
  }
  else if (!btnA_pressed && btnA_was_pressed) {
    // Button just released
    uint32_t press_duration = millis() - btnA_press_start;
    btnA_was_pressed = false;
    
    // Short press: toggle manual LED (with debouncing)
    if (press_duration > 50 && press_duration < 2000 && (millis() - last_toggle > 500)) {
      g_led_manual_local_toggle = !g_led_manual_local_toggle;
      last_toggle = millis();
      
      Serial.printf("🔘 Button A: Manual LED toggle %s\n", 
                    g_led_manual_local_toggle ? "ON" : "OFF");
      
      // Visual feedback - flash display briefly
      M5.Lcd.fillScreen(g_led_manual_local_toggle ? COL_BLUE : COL_DARK_RED);
      delay(100);
    }
  }
}

// ---- Setup/loop ----
uint32_t lastMotionSendMs=0, lastDraw=0;

void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("\nM5StickC Plus Motion + Colors + LED Control (CH=4) — met Sensitivity Menu + Rust Transitie");
  
  // Automatisch gevoeligheid instellen vanaf SENSITIVITY constante
  applySensitivity(SENSITIVITY);
  
  // Automatisch rust transitie tijd instellen vanaf REST_DELAY constante
  cfg.restDelay = max(300, min(2000, REST_DELAY));
  Serial.printf("\n🏠 RUST TRANSITIE TIJD INGESTELD OP %d ms\n", cfg.restDelay);
  Serial.printf("   Van stilstand detectie -> pijl beneden -> rode lijn\n");
  
  printHelp(); 
  printConfig();

  // M5StickC Plus initialization
  M5.begin();
  
  // Initialize LEDs early
  initLEDs();
  
  // Test LEDs during startup
  testLEDs();
  
  // LCD initialization
  M5.Lcd.setRotation(0); // Portrait mode
  M5.Lcd.fillScreen(COL_BLACK);
  M5.Lcd.setTextColor(COL_WHITE, COL_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("M5StickC Plus");
  M5.Lcd.setCursor(10, 35);
  M5.Lcd.println("Motion + LEDs");
  delay(1500); // Show startup message briefly
  
  // IMU initialisatie met check
  M5.IMU.Init();
  Serial.println("IMU Initialized");
  
  // Test IMU data direct na init
  delay(100);
  float test_ax, test_ay, test_az;
  M5.IMU.getAccelData(&test_ax, &test_ay, &test_az);
  Serial.printf("IMU Test Read: ax=%.3f, ay=%.3f, az=%.3f\n", test_ax, test_ay, test_az);
  
  if (test_ax == 0.0f && test_ay == 0.0f && test_az == 0.0f) {
    Serial.println("❌ WARNING: IMU returns all zeros! Check connection.");
  } else {
    Serial.println("✅ IMU seems to be working.");
  }

  // WiFi and ESP-NOW setup
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Kanaal 4 forceren (belangrijk!)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init()!=ESP_OK){
    M5.Lcd.fillScreen(COL_RED);
    M5.Lcd.setTextColor(COL_WHITE, COL_RED);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("ESP-NOW FAIL");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  // Add peer
  if (sscanf(HEAD_MAC_STR,"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &headMac[0],&headMac[1],&headMac[2],&headMac[3],&headMac[4],&headMac[5])==6){
    memset(&headPeer,0,sizeof(headPeer));
    memcpy(headPeer.peer_addr, headMac, 6);
    headPeer.channel = WIFI_CHANNEL;
    headPeer.encrypt = false;
    esp_now_add_peer(&headPeer);
  }

  // Bootbeeld: horizontale rode streep voor rust zodat je meteen iets ziet
  drawHorizontalLine(COL_RED, true);
  lastMotionSendMs = millis();
  lastDraw = millis();
  
  Serial.println("Setup complete - M5StickC Plus Motion + LED Controller ready!");
  Serial.printf("LEDs: GPIO %d (Vacuum), GPIO %d (Manual) | Button A = Manual Toggle\n", 
                LED_VACUUM_PIN, LED_MANUAL_PIN);
}

void loop(){
  M5.update(); // Update buttons
  pollSerialMenu();
  
  // Handle button presses for LED control
  handleButtons();

  // tijd
  static uint32_t tPrev = millis();
  uint32_t now = millis();
  float dt = (now - tPrev) / 1000.0f; 
  if (dt<0) dt=0; 
  if (dt>0.1f) dt=0.1f;
  tPrev = now;

  // IMU → dir/speed
  updateMotion(dt);
  
  // Update LED states based on commands and timeouts
  updateLEDs();

  // TX naar head volgens interval
  if (now - lastMotionSendMs >= cfg.sendInterval){
    lastMotionSendMs = now;
    sendMotion();
  }

  // ALTIJD tekenen (~30 FPS)
  if (now - lastDraw >= cfg.drawInterval){
    lastDraw = now;
    drawFromState();
  }

  delay(2);
}
