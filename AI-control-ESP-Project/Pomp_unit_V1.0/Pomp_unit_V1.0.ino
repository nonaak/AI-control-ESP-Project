// ============================================================================
//  Basis Pomp Unit V1.0  (ESP8266 + HX711 + 2 pompen + servo + lucht-relais)
// ============================================================================
// - Robuuste baseline (vastgelegd) met UI die jij goedkeurde:
//    * Twee pompkaarten: "Vacuum" en "Lube", elk met klein pomp-icoon,
//      klein motorblokje, statusdot en compacte waarden.
//    * WiFi-icoon rechtsboven: 3 bogen (1px dik, 2px gap) + 2x2 dot.
//      Bij geen verbinding: X (kruis) door het icoon (oorspronkelijke positie).
//
// - Functionaliteit
//    * Vacuümpomp ↔ servo ↔ lucht-relais zijn gekoppeld:
//         Pomp AAN  => servo DICHT, lucht DICHT
//         Pomp UIT  => servo OPEN,  lucht OPEN + (optionele) tare HX711
//    * HX711 uitlezing, instelbare TARE-wachttijd (bovenaan), default 0 ms.
//    * Lube-pomp: PRIME bij sessiestart en SHOT op punches/command.
//    * ESP-NOW compatibel met V1/V2/V3 (V3 voegt puls-commando’s toe):
//         - override force_pump_state: 0=AUTO, 1=OFF, 2=ON
//         - cmd_lube_prime_now (pulse)
//         - cmd_lube_shot_now  (pulse)
//
// - ESP-NOW setup
//    * Kanaal: 4 (moet gelijk zijn aan HoofdESP)
//    * HoofdESP MAC:  E4:65:B8:7A:85:E4
//    * PompUnit MAC:  60:01:94:59:18:86
//
// - Pinout (NodeMCU / ESP8266):
//    * I2C (OLED SH1106 128x64): Wire.begin(5,4) => SDA=D1(GPIO5), SCL=D2(GPIO4)
//    * HX711: DOUT=D7(GPIO13), SCK=D8(GPIO15)
//    * Vacuümpomp-relay: D5(GPIO14)
//    * Lube-pomp-relay:  D6(GPIO12)
//    * Servo-klep:       D3(GPIO0)
//    * Lucht-relais:     D0(GPIO16)   (LOW=open, HIGH=dicht in deze code)
// ----------------------------------------------------------------------------

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <HX711.h>
#include <Servo.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

// ==== INSTELBARE PARAMS ====
const uint16_t TARE_DELAY_MS = 0; // wachttijd na pomp UIT (ms). 0 = direct tare
const uint8_t  SERVO_OPEN_ANGLE  = 40;
const uint8_t  SERVO_CLOSE_ANGLE = 120;
const float    HX_SCALE = 77550.0f;
const long     HX_TARE_SAMPLES = 8;
const uint32_t LUBE_PRIME_MS    = 1500;
const uint32_t LUBE_SHOT_MS     = 700;
const uint32_t LUBE_COOLDOWN_MS = 400;
const uint32_t LUBE_PUNCHES_TRIGGER = 25;
const int      ESPNOW_CHANNEL=4; // Changed to match HoofdESP channel
uint8_t MAIN_MAC[6]={0xE4,0x65,0xB8,0x7A,0x85,0xE4};

// ==== PINNING ====
// ESP8266 NodeMCU pin mapping: D0=GPIO16, D1=GPIO5, D2=GPIO4, D3=GPIO0, D4=GPIO2, D5=GPIO14, D6=GPIO12, D7=GPIO13, D8=GPIO15
#define PIN_HX_DOUT    13  // D7 = GPIO13
#define PIN_HX_SCK     15  // D8 = GPIO15
#define PIN_VAC_PUMP   14  // D5 = GPIO14
#define PIN_LUBE_PUMP  12  // D6 = GPIO12
#define PIN_SERVO      0   // D3 = GPIO0
#define PIN_AIR_RELAY  16  // D0 = GPIO16

HX711  scale;
float  vac_avg = 0.0f;
float  vac_buf[6] = {0};
uint8_t vac_idx=0, vac_fill=0;

volatile bool     arrow_full=false, session_started=false;
volatile uint32_t punch_count=0, punch_goal=0;
volatile int16_t  vacuum_set_x10=-200; // tienden cmHg

bool vacuum_pump_on=false, lube_pump_on=false;

uint32_t t_now=0,t_last=0,t_lube_until=0,t_lube_cool_until=0,last_punch_seen=0;
bool     session_armed=false;

uint32_t lastRx = 0;                    // laatst Rx-tijd (ms)
volatile uint8_t force_pump_state = 0;  // 0=AUTO,1=OFF,2=ON

// V3 puls-commando's
volatile uint8_t cmd_lube_prime_now = 0;
volatile uint8_t cmd_lube_shot_now  = 0;
volatile uint8_t cmd_toggle_zuigen = 0;   // Toggle zuigen command from HoofdESP
volatile uint16_t received_lube_duration_ms = 0;  // Dynamische lube timing van HoofdESP

// Lokale zuigen state machine - runs entirely on Pump Unit
enum ZuigState { ZUIG_IDLE=0, ZUIG_ACTIEF=1 };
ZuigState zuigState = ZUIG_IDLE;
float zuigTargetVacuum = -30.0f;  // Target vacuum for zuigen mode (mbar) - matches HoofdESP default
uint32_t zuigStateTime = 0;       // When current state started
uint32_t zuigCooldownUntil = 0;   // Prevent auto-vacuum after stopping zuigen
// zuigHoldTime verwijderd - 100% handmatige controle

// Telemetrie timing
const uint32_t TELEMETRY_INTERVAL_MS = 250; // 250ms = 4Hz voor real-time vacuum feedback
uint32_t last_telemetry_tx = 0;
uint32_t system_start_time = 0;

Servo     vacServo;
bool      tare_pending = false;
uint32_t  tare_at_ms   = 0;

// ---- ESPNOW payloads ----
// Incoming messages (from HoofdESP)
struct __attribute__((packed)) MainMsgV1 {
  uint8_t  version;           
  uint8_t  arrow_full;
  uint8_t  session_started;
  uint32_t punch_count;
  uint32_t punch_goal;
  int16_t  vacuum_set_x10;
};
struct __attribute__((packed)) MainMsgV2 {
  uint8_t  version;           
  uint8_t  arrow_full;
  uint8_t  session_started;
  uint32_t punch_count;
  uint32_t punch_goal;
  int16_t  vacuum_set_x10;
  uint8_t  force_pump_state;  
};
struct __attribute__((packed)) MainMsgV3 {
  uint8_t  version;           
  uint8_t  arrow_full;
  uint8_t  session_started;
  uint32_t punch_count;
  uint32_t punch_goal;
  int16_t  vacuum_set_x10;
  uint8_t  force_pump_state;  
  uint8_t  cmd_lube_prime_now;// 0/1 pulse
  uint8_t  cmd_lube_shot_now; // 0/1 pulse
  uint16_t lube_duration_ms;  // Lube timing in milliseconden (0-20000ms)
  uint8_t  cmd_toggle_zuigen; // Pulse commando: toggle zuigen mode (0/1)
  int16_t  zuig_target_x10;   // Zuig target vacuum (tienden cmHg)
};

// Outgoing telemetry message (to HoofdESP)
struct __attribute__((packed)) PumpStatusMsg {
  uint8_t  version;              // Message version (1)
  float    current_vacuum_cmHg;  // HX711 vacuum reading
  uint8_t  vacuum_pump_on;       // Vacuum pump status (0/1)
  uint8_t  lube_pump_on;         // Lube pump status (0/1)
  uint8_t  servo_open;           // Servo valve position (0/1)
  uint8_t  air_relay_open;       // Air relay status (0/1)
  float    lube_remaining_sec;   // Remaining lube time in seconds
  char     system_status[8];     // "OK"/"TARE"/"ERROR"
  uint8_t  force_pump_state;     // Current force state (0=AUTO,1=OFF,2=ON)
  uint8_t  zuig_active;          // Zuigen mode active on Pump Unit (0/1)
  uint32_t uptime_sec;           // System uptime in seconds
};

float rollingAverage(float x){
  vac_buf[vac_idx]=x; vac_idx=(vac_idx+1)%6; if(vac_fill<6)vac_fill++;
  float s=0; for(uint8_t i=0;i<vac_fill;i++) s+=vac_buf[i];
  return s/(vac_fill?vac_fill:1);
}

void onDataRecv(uint8_t *mac,uint8_t *incomingData,uint8_t len){
  // Debug ESP-NOW ontvangst
  Serial.printf("[ESP-NOW RX] From: %02X:%02X:%02X:%02X:%02X:%02X, Len: %d\n",
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], len);
  
  if(len >= sizeof(MainMsgV3)) {
    MainMsgV3 msg; memcpy(&msg,incomingData,sizeof(MainMsgV3));
    Serial.printf("[ESP-NOW RX] V3 check: version=%d\n", msg.version);
    if(msg.version==3){
      arrow_full=msg.arrow_full;
      static uint8_t last_start=0;
      if(msg.session_started && !last_start) session_started=true;
      last_start=msg.session_started;
      punch_count=msg.punch_count; punch_goal=msg.punch_goal; vacuum_set_x10=msg.vacuum_set_x10;
      force_pump_state = msg.force_pump_state;
      if(msg.cmd_lube_prime_now) cmd_lube_prime_now = 1;
      if(msg.cmd_lube_shot_now)  cmd_lube_shot_now  = 1;
      if(msg.cmd_toggle_zuigen)  cmd_toggle_zuigen  = 1;  // Toggle zuigen command
      received_lube_duration_ms = msg.lube_duration_ms;  // Store dynamic timing
      zuigTargetVacuum = (msg.zuig_target_x10 / 10.0f) / 0.75f;  // Convert from cmHg to mbar (1 mbar = 0.75 cmHg)
      lastRx = millis();
      // Debug conversie verificatie
      float targetCmHg = vacuum_set_x10 / 10.0f;
      float targetMbar = targetCmHg / 0.75f;  // Convert to mbar
      int targetDisplay = (int)abs(targetMbar);
      
      Serial.printf("[RX] ✅ V3: arrow=%d, punch=%u, vac_set=%.1f cmHg (Display: %d mbar), toggle_zuig=%d\n", 
                    arrow_full, punch_count, targetCmHg, targetDisplay, msg.cmd_toggle_zuigen);
      Serial.printf("[CONVERSION] %.1f cmHg -> %.0f mbar -> Display: %d\n", targetCmHg, targetMbar, targetDisplay);
      return;
    }
  }
  if(len >= sizeof(MainMsgV2)) {
    MainMsgV2 msg; memcpy(&msg,incomingData,sizeof(MainMsgV2));
    if(msg.version==2){
      arrow_full=msg.arrow_full;
      static uint8_t last_start=0;
      if(msg.session_started && !last_start) session_started=true;
      last_start=msg.session_started;
      punch_count=msg.punch_count; punch_goal=msg.punch_goal; vacuum_set_x10=msg.vacuum_set_x10;
      force_pump_state = msg.force_pump_state;
      lastRx = millis();
      return;
    }
  }
  if(len >= sizeof(MainMsgV1)) {
    MainMsgV1 msg; memcpy(&msg,incomingData,sizeof(MainMsgV1));
    if(msg.version==1){
      arrow_full=msg.arrow_full;
      static uint8_t last_start=0;
      if(msg.session_started && !last_start) session_started=true;
      last_start=msg.session_started;
      punch_count=msg.punch_count; punch_goal=msg.punch_goal; vacuum_set_x10=msg.vacuum_set_x10;
      force_pump_state = 0;
      lastRx = millis();
    }
  }
}

void setServoOpen(bool open){ vacServo.write(open ? SERVO_OPEN_ANGLE : SERVO_CLOSE_ANGLE); }
// LOW=open, HIGH=dicht (pas dit om indien jouw relais andersom is)
void setAirRelayOpen(bool open){ digitalWrite(PIN_AIR_RELAY, open ? LOW : HIGH); }
void lubePumpSet(bool on){ lube_pump_on=on; digitalWrite(PIN_LUBE_PUMP,on); }

void vacuumSystemSet(bool on){
  vacuum_pump_on = on;
  digitalWrite(PIN_VAC_PUMP, on);
  
  if(on){
    // Pump AAN - altijd kleppen dicht
    setServoOpen(false);   // servo dicht
    setAirRelayOpen(false);// lucht dicht
    tare_pending = false;
  } else {
    // Pump UIT - kleppen behavior hangt af van zuigen mode
    if(zuigState != ZUIG_IDLE) {
      // ZUIGEN ACTIEF - houd kleppen DICHT ook al is pomp uit (target bereikt)
      setServoOpen(false);   // servo BLIJFT dicht
      setAirRelayOpen(false);// lucht BLIJFT dicht
      Serial.println("[VACUUM] Pump OFF but keeping valves CLOSED (zuigen active)");
    } else {
      // NORMALE MODE - kleppen open bij pomp uit
      setServoOpen(true);    // servo open
      setAirRelayOpen(true); // lucht open
      
      // Tare HX711 na pomp uit (alleen in normale mode)
      if(TARE_DELAY_MS==0){
        if(scale.is_ready()) scale.tare(HX_TARE_SAMPLES);
        tare_pending=false;
      }else{
        tare_pending = true;
        tare_at_ms   = millis() + TARE_DELAY_MS;
      }
    }
  }
}

void failsafeVac(){ vacuumSystemSet(false); }

// Telemetrie: verstuur status update naar HoofdESP
void sendStatusUpdate(){
  PumpStatusMsg status;
  
  // Vul status bericht
  status.version = 1;
  status.current_vacuum_cmHg = vac_avg;  // HX711 vacuum waarde!
  status.vacuum_pump_on = vacuum_pump_on ? 1 : 0;
  status.lube_pump_on = lube_pump_on ? 1 : 0;
  status.servo_open = (vacServo.read() == SERVO_OPEN_ANGLE) ? 1 : 0;
  status.air_relay_open = digitalRead(PIN_AIR_RELAY) == LOW ? 1 : 0; // LOW=open
  
  // Resterende lube tijd
  if(lube_pump_on && t_lube_until > t_now) {
    status.lube_remaining_sec = (t_lube_until - t_now) / 1000.0f;
  } else {
    status.lube_remaining_sec = 0.0f;
  }
  
  // Systeem status - verbeterde HX711 check
  if(tare_pending) {
    strcpy(status.system_status, "TARE");
  } else {
    // HX711 status: alleen ERROR als echt geen data beschikbaar
    static uint32_t last_hx_check = 0;
    static bool hx_status_ok = true;
    
    // Check HX711 status niet elke 250ms, maar minder frequent
    if(millis() - last_hx_check > 1000) { // Elke seconde
      bool new_hx_status = scale.is_ready();
      if(new_hx_status != hx_status_ok) {
        Serial.printf("[HX711] Status change: %s\n", new_hx_status ? "OK" : "ERROR");
      }
      hx_status_ok = new_hx_status;
      last_hx_check = millis();
    }
    
    // Extra check: hebben we recent geldige data gehad?
    static uint32_t last_valid_reading = 0;
    if(millis() - last_valid_reading < 5000) { // Geldige data binnen 5 seconden
      strcpy(status.system_status, "OK");
    } else if(!hx_status_ok) {
      strcpy(status.system_status, "ERROR");
    } else {
      strcpy(status.system_status, "OK");
    }
    
    // Update last_valid_reading als we net geldige data hebben gelezen
    if(!isnan(vac_avg) && !isinf(vac_avg)) {
      last_valid_reading = millis();
    }
  }
  
  status.force_pump_state = force_pump_state;
  status.zuig_active = (zuigState != ZUIG_IDLE) ? 1 : 0;  // Report zuigen status to HoofdESP
  status.uptime_sec = (millis() - system_start_time) / 1000;
  
  // Verstuur via ESP-NOW (ESP8266 versie)
  uint8_t result = esp_now_send(MAIN_MAC, (uint8_t*)&status, sizeof(status));
  
  // Debug output
  if(result == 0) {
    Serial.printf("[TX] Vacuum: %.1f cmHg, VacPump:%d, LubePump:%d, Status:%s\n", 
                  status.current_vacuum_cmHg, status.vacuum_pump_on, status.lube_pump_on, status.system_status);
  } else {
    Serial.printf("[TX] Error: %d\n", result);
  }
}

// --- UI helpers (zoals goedgekeurd) ---
void drawSimplePumpIcon(int x,int y,int w,int h,bool active,bool phase,uint8_t bladeIdx){
  u8g2.drawRFrame(x,y,w,h,3);
  int cx=x+w/3, cy=y+h/2, r=max(3,min(w,h)/5);
  u8g2.drawCircle(cx,cy,r,U8G2_DRAW_ALL);
  switch(bladeIdx%3){
    case 0: u8g2.drawLine(cx,cy,cx+r,cy); u8g2.drawLine(cx,cy,cx-r/2,cy-r); u8g2.drawLine(cx,cy,cx-r/2,cy+r); break;
    case 1: u8g2.drawLine(cx,cy,cx,cy-r); u8g2.drawLine(cx,cy,cx+r,cy+r/3); u8g2.drawLine(cx,cy,cx-r,cy+r/3); break;
    default:u8g2.drawLine(cx,cy,cx-r,cy); u8g2.drawLine(cx,cy,cx+r/2,cy-r); u8g2.drawLine(cx,cy,cx+r/2,cy+r); break;
  }
  // klein motorblokje
  int mx=x+w-12, my=y+h/4, mw=8, mh=h/2;
  if(active&&phase) u8g2.drawBox(mx,my,mw,mh); else u8g2.drawFrame(mx,my,mw,mh);
  int ax=x+w-3, ay=y+h/2; u8g2.drawTriangle(ax,ay,ax-5,ay-3,ax-5,ay+3);
}
void drawStatusDot(int cx,int cy,bool on,uint32_t t_ms,uint16_t period=240){
  if(!on){u8g2.drawCircle(cx,cy,2,U8G2_DRAW_ALL); return;}
  bool phase=((t_ms/(period/2))%2)==0; uint8_t r=phase?3:2;
  u8g2.drawDisc(cx,cy,r);
}
void drawWiFiIcon(bool connected){
  const int cx = 116; // 4 px naar links van de rand (past mooi)
  const int cy = 8;
  const int R1 = 8, R2 = 5, R3 = 2;
  const int TH = 1;
  auto ring1px = [&](int r){ u8g2.setDrawColor(1); u8g2.drawDisc(cx, cy, r);
                             u8g2.setDrawColor(0); u8g2.drawDisc(cx, cy, r-TH);
                             u8g2.setDrawColor(1); };
  ring1px(R1); ring1px(R2); ring1px(R3);
  u8g2.setDrawColor(0); u8g2.drawBox(cx-(R1+1), cy, 2*(R1+1), R1+6); u8g2.setDrawColor(1);
  u8g2.drawBox(cx-1, cy+2, 2, 2);

  if(!connected){
    // originele, nette kruis-positie
    u8g2.drawLine(cx-6, cy-1,  cx+6, cy+11);
    u8g2.drawLine(cx-6, cy+11, cx+6, cy-1);
  }
}

void drawUI(float vac_cmHg){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_lubBI10_tf);
  u8g2.drawStr(2,12,"Pump Unit");
  bool connected = (millis() - lastRx < 3000);
  drawWiFiIcon(connected);

  const int cardW=60, cardH=48, leftX=2, rightX=66, topY=14;
  bool    vacPhase=((t_now/180)%2)==0, lubePhase=((t_now/240)%2)==0;
  uint8_t vacBlade=(t_now/120)%3,       lubeBlade=(t_now/150)%3;

  // VAC
  u8g2.drawRFrame(leftX,topY,cardW,cardH,4);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(leftX+6,topY+12); u8g2.print("Vacuum");
  int icoX=leftX+4, icoY=topY+16, icoW=26, icoH=18;
  drawSimplePumpIcon(icoX,icoY,icoW,icoH,vacuum_pump_on,vacPhase,vacBlade);
  drawStatusDot(icoX+icoW/2, icoY+icoH+6, vacuum_pump_on, t_now, 220);
  
  // Toon actuele vacuum waarde in mbar met hele getallen
  float vac_mbar = vac_cmHg / 0.75f;  // Convert cmHg naar mbar (1 mbar = 0.75 cmHg)
  char buf[12]; 
  snprintf(buf, sizeof(buf), "%.0f", abs(vac_mbar));  // Actuele waarde in mbar
  u8g2.setCursor(leftX+34, topY+32); u8g2.print(buf);

  // LUBE
  u8g2.drawRFrame(rightX,topY,cardW,cardH,4);
  u8g2.setCursor(rightX+6,topY+12); u8g2.print("Lube");
  int lx=rightX+4, ly=topY+16, lw=26, lh=18;
  drawSimplePumpIcon(lx,ly,lw,lh,lube_pump_on,lubePhase,lubeBlade);
  drawStatusDot(lx+lw/2, ly+lh+6, lube_pump_on, t_now, 280);
  float sec = max<int32_t>(0,(int32_t)(t_lube_until - t_now))/1000.0f;
  u8g2.setCursor(rightX+34, topY+32); u8g2.print(sec, 1);

  u8g2.sendBuffer();
}

void setup(){
  Serial.begin(115200);
  system_start_time = millis(); // Voor uptime tracking

  pinMode(PIN_VAC_PUMP,OUTPUT);
  pinMode(PIN_LUBE_PUMP,OUTPUT);
  pinMode(PIN_AIR_RELAY,OUTPUT);

  digitalWrite(PIN_VAC_PUMP, LOW);
  digitalWrite(PIN_LUBE_PUMP, LOW);
  setAirRelayOpen(true);

  vacServo.attach(PIN_SERVO);
  setServoOpen(true);

  Wire.begin(5,4);
  u8g2.begin();
  //uu8g22e(1);  // 1 = 180 graden flip, 0 = normaal

  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_lubBI12_tf);
  u8g2.drawStr(2,24,"Init WiFi ch4..."); u8g2.sendBuffer();

  // ESP8266 WiFi setup voor ESP-NOW (channel 4)
  WiFi.mode(WIFI_STA);  // Eerst STA mode
  WiFi.disconnect();
  delay(100);
  
  // ESP-NOW initialisatie
  if(esp_now_init()!=0){
    u8g2.clearBuffer(); u8g2.drawStr(2,24,"ESP-NOW FAIL"); u8g2.sendBuffer();
    Serial.println("[ERROR] ESP-NOW init failed!");
    while(1) delay(1000);
  }
  
  // ESP8266 ESP-NOW setup (oude API)
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  int result = esp_now_add_peer(MAIN_MAC,ESP_NOW_ROLE_CONTROLLER,ESPNOW_CHANNEL,NULL,0);
  Serial.printf("[ESP-NOW] Add peer result: %d (0=OK)\n", result);
  
  esp_now_register_recv_cb(onDataRecv);
  
  // Channel forceren na ESP-NOW setup
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("PUMP_CH4_KEEPER","",ESPNOW_CHANNEL,false);
  
  Serial.printf("[ESP-NOW] ESP8266 setup complete on channel %d\n", ESPNOW_CHANNEL);
  Serial.printf("[ESP-NOW] Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                MAIN_MAC[0],MAIN_MAC[1],MAIN_MAC[2],MAIN_MAC[3],MAIN_MAC[4],MAIN_MAC[5]);

  scale.begin(PIN_HX_DOUT,PIN_HX_SCK);
  scale.set_scale(HX_SCALE);
  delay(150);
  scale.tare(HX_TARE_SAMPLES);

  float v0=scale.is_ready()?scale.get_units(5):0.0f;
  for(uint8_t i=0;i<6;i++)vac_buf[i]=v0; vac_fill=6; vac_idx=0; vac_avg=v0;

  u8g2.clearBuffer(); u8g2.drawStr(2,24,"Gereed"); u8g2.sendBuffer(); delay(300);
  
  Serial.println(F("\n[PumpUnit] System ready!"));
  Serial.printf("[PumpUnit] Telemetrie elke %dms (%.1fHz) naar HoofdESP\n", TELEMETRY_INTERVAL_MS, 1000.0f/TELEMETRY_INTERVAL_MS);
  Serial.printf("[PumpUnit] PumpStatusMsg size: %d bytes\n", sizeof(PumpStatusMsg));
}

void loop(){
  t_now=millis();
  if(t_now - t_last < 20){ delay(1); return; }
  t_last=t_now;

  // geplande tare
  if(tare_pending && (millis() >= tare_at_ms)){
    if(scale.is_ready()){
      scale.tare(HX_TARE_SAMPLES);
      tare_pending = false;
      vac_avg = 0.0f;  // Reset rolling average to 0
      Serial.println(F("[PumpUnit] ✅ HX711 tared - measurement reset to 0"));
    }
  }

  // More robust HX711 reading with recovery
  static uint32_t lastHXAttempt = 0;
  static uint32_t lastSuccessfulRead = millis();
  static uint8_t consecutiveFailures = 0;
  
  if(millis() - lastHXAttempt > 100) { // Don't spam HX711 checks
    lastHXAttempt = millis();
    
    if(scale.is_ready()){
      float v = scale.get_units(2);
      if(!isnan(v) && !isinf(v) && fabs(v) < 100.0f) { // Reasonable vacuum range check
        if(fabs(v - vac_avg) < 30.0f || consecutiveFailures > 3) { // Accept if close OR if we're having issues
          vac_avg = rollingAverage(v);
          lastSuccessfulRead = millis();
          consecutiveFailures = 0;
          
          // Debug successful reads occasionally
          static uint32_t lastGoodDebug = 0;
          if(millis() - lastGoodDebug > 2000) {
            lastGoodDebug = millis();
            Serial.printf("[HX711] Good reading: %.1f cmHg\n", v);
          }
        } else {
          consecutiveFailures++;
          Serial.printf("[HX711] Suspicious reading: %.1f (avg: %.1f, failures: %d)\n", v, vac_avg, consecutiveFailures);
        }
      } else {
        consecutiveFailures++;
        Serial.printf("[HX711] Invalid reading: %.1f (failures: %d)\n", v, consecutiveFailures);
      }
    } else {
      consecutiveFailures++;
      if(consecutiveFailures % 10 == 0) { // Report every 10 failures
        Serial.printf("[HX711] Not ready (failures: %d, last good: %lums ago)\n", 
                      consecutiveFailures, millis() - lastSuccessfulRead);
        
        // Try to recover HX711 after many failures
        if(consecutiveFailures > 50) {
          Serial.println("[HX711] Attempting recovery - re-initializing...");
          scale.begin(PIN_HX_DOUT, PIN_HX_SCK);
          scale.set_scale(HX_SCALE);
          consecutiveFailures = 30; // Reset but keep some failure count
        }
      }
    }
  }

  // V3 Toggle zuigen commando - lokale state machine
  if(cmd_toggle_zuigen) {
    cmd_toggle_zuigen = 0;
    
    if(zuigState == ZUIG_IDLE) {
      // Start zuigen
      zuigState = ZUIG_ACTIEF;
      zuigStateTime = millis();
      Serial.printf("[ZUIG] ✅ STARTING zuigen mode - Target: %.0f mbar (received from HoofdESP)\n", abs(zuigTargetVacuum));
      Serial.printf("[ZUIG] Current vacuum: %.0f mbar, Adaptive hysteresis\n", abs(vac_avg / 0.75f));
    } else {
      // Stop zuigen - open kleppen en reset naar idle
      zuigState = ZUIG_IDLE;
      zuigStateTime = millis();
      
      // Force kleppen open bij stop
      setServoOpen(true);    // servo open
      setAirRelayOpen(true); // lucht open
      
      // Stop vacuum pump
      if(vacuum_pump_on) {
        vacuumSystemSet(false);
      }
      
      // Schedule immediate HX711 tare to reset hardware measurement to 0
      if(!tare_pending) {
        tare_pending = true;
        tare_at_ms = millis() + 1000;  // Tare after 1 second (kleppen zijn dan open)
        Serial.println("[ZUIG] Scheduling HX711 tare in 1s to reset measurement");
      }
      
      // CRISPY: Shorter cooldown for faster auto vacuum response after zuigen
      zuigCooldownUntil = millis() + 1000;  // 1 second cooldown (was 3s)
      
      Serial.println("[ZUIG] ❌ STOPPING zuigen mode - valves opened, HX711 tare scheduled");
    }
  }
  
  // Lokale zuigen state machine - 100% handmatig, geen automatische stops
  // Zuigen blijft actief tot handmatig gestopt met Z-knop toggle
  // Alleen ZUIG_IDLE en ZUIG_ACTIEF states

  // V3 puls-commando's
  if(cmd_lube_prime_now){ cmd_lube_prime_now=0;
    if(t_now>=t_lube_cool_until){
      uint32_t lube_duration = (received_lube_duration_ms > 0) ? received_lube_duration_ms : LUBE_PRIME_MS;
      t_lube_until = t_now + lube_duration;
      t_lube_cool_until = t_lube_until + LUBE_COOLDOWN_MS;
      lubePumpSet(true);
      session_armed=true;
      Serial.printf("[PumpUnit] Lube PRIME (cmd) for %ums\n", lube_duration);
    }
  }
  if(cmd_lube_shot_now ){ cmd_lube_shot_now =0;
    if(t_now>=t_lube_cool_until){
      uint32_t lube_duration = (received_lube_duration_ms > 0) ? received_lube_duration_ms : LUBE_SHOT_MS;
      t_lube_until = t_now + lube_duration;
      t_lube_cool_until = t_lube_until + LUBE_COOLDOWN_MS;
      lubePumpSet(true);
      Serial.printf("[PumpUnit] Lube SHOT (cmd) for %ums\n", lube_duration);
    }
  }

  // Lube via session/punches
  if(session_started){
    session_started=false;
    if(t_now>=t_lube_cool_until){
      t_lube_until = t_now + LUBE_PRIME_MS;
      t_lube_cool_until = t_lube_until + LUBE_COOLDOWN_MS;
      lubePumpSet(true);
      session_armed=true;
      Serial.println(F("[PumpUnit] Lube PRIME"));
    }
  }
  uint32_t goal = (punch_goal>0)? punch_goal : LUBE_PUNCHES_TRIGGER;
  static uint32_t last_seen=0;
  if(session_armed && punch_count!=last_seen){
    last_seen=punch_count;
    if(punch_count>0 && (punch_count % goal)==0){
      if(t_now>=t_lube_cool_until){
        uint32_t lube_duration = (received_lube_duration_ms > 0) ? received_lube_duration_ms : LUBE_SHOT_MS;
        t_lube_until = t_now + lube_duration;
        t_lube_cool_until = t_lube_until + LUBE_COOLDOWN_MS;
        lubePumpSet(true);
        Serial.printf("[PumpUnit] Lube SHOT (auto) for %ums\n", lube_duration);
      }
    }
  }
  if(lube_pump_on && t_now>=t_lube_until) lubePumpSet(false);

  // Vacuümregeling met override
  if(force_pump_state == 2){            // FORCE_ON
    if(!vacuum_pump_on) vacuumSystemSet(true);
  } else if(force_pump_state == 1){     // FORCE_OFF
    if(vacuum_pump_on)  vacuumSystemSet(false);
  } else {
    // AUTO - Handle both arrow_full (auto) and zuigen mode (manual)
    float set_cmHg = vacuum_set_x10 / 10.0f;
    
    // LOKALE ZUIGEN STATE MACHINE has priority over arrow_full auto vacuum
    if(zuigState != ZUIG_IDLE) {
      // Manual zuigen mode - use zuigTargetVacuum (mbar)
      float zuig_target = zuigTargetVacuum;  // Already in mbar
      float vac_avg_mbar = vac_avg / 0.75f;   // Convert current reading to mbar (1 mbar = 0.75 cmHg)
      
      static uint32_t lastZuigDebug = 0;
      if(millis() - lastZuigDebug > 1000) { // Debug elke seconde
        lastZuigDebug = millis();
        Serial.printf("[ZUIG CTRL] state=%d, vac_avg=%.0f mbar, target=%.0f mbar\n", 
                      zuigState, abs(vac_avg_mbar), abs(zuig_target));
      }
      
      if(scale.is_ready() || (millis() - lastSuccessfulRead < 10000)) {
        // Adaptieve hysteresis voor delicate regeling (mbar waarden)
        float hysteresis = max(1.0f, abs(zuig_target) * 0.1f);  // Min 1.0 mbar, max 10% van target
        if(!vacuum_pump_on && vac_avg_mbar > (zuig_target + hysteresis)) {
          Serial.printf("[ZUIG] Starting vacuum: %.0f > %.0f (hysteresis +%.0f mbar)\n", abs(vac_avg_mbar), abs(zuig_target + hysteresis), hysteresis);
          vacuumSystemSet(true);
        }
        else if(vacuum_pump_on && vac_avg_mbar < (zuig_target - hysteresis)) {
          Serial.printf("[ZUIG] Stopping vacuum: %.0f < %.0f (hysteresis -%.0f mbar, KLEPPEN BLIJVEN DICHT)\n", abs(vac_avg_mbar), abs(zuig_target - hysteresis), hysteresis);
          vacuumSystemSet(false);  // Pomp uit, maar kleppen blijven dicht!
        }
      } else {
        Serial.printf("[ZUIG] No valid HX711 readings - failsafe!\n");
        failsafeVac();
      }
    } else if(arrow_full && millis() > zuigCooldownUntil){
      // AUTO VACUUM - SIMPLE FOLLOW MODE (no mbar sensor logic)
      // Just follow arrow_full signal directly from HoofdESP
      if(!vacuum_pump_on) {
        Serial.printf("[AUTO-VAC] ✅ Starting vacuum (arrow_full=1)\n");
        vacuumSystemSet(true);
      }
    } else if(arrow_full && millis() <= zuigCooldownUntil){
      // Auto-vacuum blocked by zuigen cooldown (now only 1 second)
      static uint32_t lastCooldownDebug = 0;
      if(millis() - lastCooldownDebug > 500) { // Debug every 0.5s during short cooldown
        lastCooldownDebug = millis();
        uint32_t remaining = (zuigCooldownUntil - millis() + 500) / 1000; // Round up
        Serial.printf("[AUTO-VAC] COOLDOWN: %us remaining\n", remaining);
      }
    } else {
      // arrow_full = 0 or still in cooldown - turn off vacuum pump
      if(vacuum_pump_on) {
        Serial.printf("[AUTO-VAC] ❌ Stopping vacuum (arrow_full=0)\n");
        vacuumSystemSet(false);
      }
    }
  }

  // Periodieke telemetrie naar HoofdESP
  if(t_now - last_telemetry_tx >= TELEMETRY_INTERVAL_MS) {
    last_telemetry_tx = t_now;
    sendStatusUpdate();
  }

  drawUI(vac_avg);
}


