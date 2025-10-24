#include "ai_event_config_view.h"
#include <Arduino_GFX_Library.h>
#include <EEPROM.h>
#include "input_touch.h"

// Global config instance
AIEventConfig aiEventConfig;

static Arduino_GFX* g = nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_SAVE=0x07E0;      // Groen voor opslaan
static const uint16_t COL_BTN_RESET=0xF800;     // Rood voor reset
static const uint16_t COL_BTN_BACK=0x001F;      // Blauw voor terug
static const uint16_t COL_BTN_EDIT=0xFFE0;      // Geel voor bewerken
static const uint16_t COL_BTN_NEXT=0xF81F;      // Magenta voor volgende
static const uint16_t COL_BTN_PREV=0x780F;      // Paars voor vorige
static const uint16_t COL_EVENT_BG=0x4208;      // Donkerder grijs voor event namen

static int16_t SCR_W=320, SCR_H=240;
static uint32_t lastTouchMs = 0;
static const uint16_t CONFIG_COOLDOWN_MS = 800;  // Consistente cooldown

// Button areas
static int16_t btnSaveX, btnSaveY, btnResetX, btnResetY, btnBackX, btnBackY;
static int16_t btnNextX, btnNextY, btnPrevX, btnPrevY;
static const int16_t BTN_W = 60, BTN_H = 25;

// Edit state
static uint8_t currentPage = 0;  // 0=events 0-3, 1=events 4-7
static int8_t editingEvent = -1; // -1 = niet aan het bewerken
static String editBuffer = "";

// Default event names
static const char* defaultEventNames[8] = {
  "Hoge hartslag gedetecteerd",
  "Temperatuur boven drempel", 
  "GSR stress indicator",
  "Lage hartslag waarschuwing",
  "Machine snelheidspieken",
  "Combinatie stress signalen",
  "Onregelmatige hartslag",
  "Langdurige stress periode"
};

// EEPROM address voor AI event config (na sensor config)
#define AI_EVENT_CONFIG_EEPROM_ADDR 600

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  g->fillRect(x,y,w,h,color);
  g->drawRect(x,y,w,h,COL_FR);
  g->setTextColor(BLACK);
  g->setTextSize(1);
  int16_t tw=strlen(txt)*6;
  g->setCursor(x+(w-tw)/2, y+(h-8)/2);
  g->print(txt);
}

static void drawEventLine(int16_t y, uint8_t eventIndex, const char* eventName, bool isEditing) {
  // Event nummer
  g->setTextColor(COL_TX);
  g->setTextSize(1);
  g->setCursor(10, y);
  g->printf("%d:", eventIndex + 1);
  
  // Event naam box
  uint16_t bgColor = isEditing ? 0x07E0 : COL_EVENT_BG;  // Groen als aan het bewerken
  g->fillRect(30, y-4, 200, 18, bgColor);
  g->drawRect(30, y-4, 200, 18, COL_FR);
  
  // Event naam tekst
  g->setTextColor(isEditing ? BLACK : WHITE);
  g->setCursor(35, y+1);
  
  // Truncate lange namen
  String displayName = String(eventName);
  if (displayName.length() > 25) {
    displayName = displayName.substring(0, 22) + "...";
  }
  g->print(displayName);
  
  // Edit knop
  drawButton(240, y-4, 50, 18, "EDIT", COL_BTN_EDIT);
}

void aiEventConfig_loadFromEEPROM() {
  EEPROM.begin(1024);  // Grotere EEPROM size voor beide configs
  
  AIEventConfig loaded;
  EEPROM.get(AI_EVENT_CONFIG_EEPROM_ADDR, loaded);
  
  if (loaded.magic == 0xABCD1234) {
    aiEventConfig = loaded;
  } else {
    // First time - use defaults
    aiEventConfig_resetToDefaults();
    aiEventConfig_saveToEEPROM();
  }
}

void aiEventConfig_saveToEEPROM() {
  aiEventConfig.magic = 0xABCD1234;
  EEPROM.put(AI_EVENT_CONFIG_EEPROM_ADDR, aiEventConfig);
  EEPROM.commit();
}

void aiEventConfig_resetToDefaults() {
  for (int i = 0; i < 8; i++) {
    strncpy(aiEventConfig.eventNames[i], defaultEventNames[i], 63);
    aiEventConfig.eventNames[i][63] = '\0';
  }
}

const char* aiEventConfig_getName(uint8_t eventType) {
  if (eventType >= 8) return "Onbekende gebeurtenis";
  return aiEventConfig.eventNames[eventType];
}

void aiEventConfig_begin(Arduino_GFX* gfx) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  currentPage = 0;
  editingEvent = -1;
  
  g->fillScreen(COL_BG);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  g->setCursor(10, 12);
  g->print("AI Gebeurtenis Config");
  
  // Page info
  g->setTextSize(1);
  g->setCursor(10, 35);
  g->printf("Pagina %d/2 - Events %d-%d", currentPage + 1, currentPage * 4 + 1, (currentPage + 1) * 4);
  
  // Draw events voor huidige pagina
  int startEvent = currentPage * 4;
  for (int i = 0; i < 4; i++) {
    int eventIndex = startEvent + i;
    int yPos = 55 + i * 25;
    bool isEditing = (editingEvent == eventIndex);
    
    const char* eventName = isEditing ? editBuffer.c_str() : aiEventConfig.eventNames[eventIndex];
    drawEventLine(yPos, eventIndex, eventName, isEditing);
  }
  
  // Instructions
  g->setTextColor(0xFFE0);  // Geel
  g->setCursor(10, 160);
  if (editingEvent >= 0) {
    g->print("Bewerken: Typ nieuwe naam en druk SAVE");
  } else {
    g->print("Druk EDIT om gebeurtenis naam te wijzigen");
  }
  
  // Bottom buttons
  int16_t btnY = SCR_H - BTN_H - 8;
  int16_t spacing = (SCR_W - 5 * BTN_W) / 6;
  btnPrevX = spacing; btnPrevY = btnY;
  btnNextX = btnPrevX + BTN_W + spacing; btnNextY = btnY;
  btnSaveX = btnNextX + BTN_W + spacing; btnSaveY = btnY;
  btnResetX = btnSaveX + BTN_W + spacing; btnResetY = btnY;
  btnBackX = btnResetX + BTN_W + spacing; btnBackY = btnY;
  
  uint16_t prevColor = (currentPage > 0) ? COL_BTN_PREV : 0x39E7;  // Grijs als disabled
  uint16_t nextColor = (currentPage < 1) ? COL_BTN_NEXT : 0x39E7;  // Grijs als disabled
  
  drawButton(btnPrevX, btnPrevY, BTN_W, BTN_H, "VORIGE", prevColor);
  drawButton(btnNextX, btnNextY, BTN_W, BTN_H, "VOLGENDE", nextColor);
  drawButton(btnSaveX, btnSaveY, BTN_W, BTN_H, "OPSLAAN", COL_BTN_SAVE);
  drawButton(btnResetX, btnResetY, BTN_W, BTN_H, "RESET", COL_BTN_RESET);
  drawButton(btnBackX, btnBackY, BTN_W, BTN_H, "TERUG", COL_BTN_BACK);
}

// Simpele text input simulatie (voor demo)
static void handleTextInput(uint8_t eventIndex) {
  // Voor nu een simpele cycling door enkele voorbeeldteksten
  static const char* exampleTexts[] = {
    "Hartslag te hoog - interventie nodig",
    "Temperatuurstijging gedetecteerd", 
    "Stress niveau kritiek - verlaag intensiteit",
    "Hartslag te laag - controleer sensoren",
    "Machine snelheid piek - aanpassing nodig",
    "Meerdere stress factoren actief",
    "Hartritme onregelmatig - let op",
    "Stress periode te lang - onderbreek sessie"
  };
  
  static uint8_t textIndex[8] = {0};
  
  // Cycle naar volgende example text
  textIndex[eventIndex] = (textIndex[eventIndex] + 1) % 3;  // 3 varianten per event type
  
  switch (eventIndex) {
    case 0: // Hoge hartslag
      if (textIndex[0] == 0) editBuffer = "Hartslag te hoog - interventie nodig";
      else if (textIndex[0] == 1) editBuffer = "Hoge hartslag gedetecteerd";
      else editBuffer = "Hartfrequentie boven drempel";
      break;
    case 1: // Temperatuur
      if (textIndex[1] == 0) editBuffer = "Temperatuurstijging gedetecteerd";
      else if (textIndex[1] == 1) editBuffer = "Lichaamstemperatuur te hoog";
      else editBuffer = "Temperatuur boven veilige grens";
      break;
    case 2: // GSR
      if (textIndex[2] == 0) editBuffer = "Stress niveau kritiek - verlaag intensiteit";
      else if (textIndex[2] == 1) editBuffer = "Huidgeleidingsweerstand verhoogd";  
      else editBuffer = "GSR stress indicator actief";
      break;
    case 3: // Lage hartslag
      if (textIndex[3] == 0) editBuffer = "Hartslag te laag - controleer sensoren";
      else if (textIndex[3] == 1) editBuffer = "Lage hartfrequentie waarschuwing";
      else editBuffer = "Hartslag onder minimum drempel";
      break;
    case 4: // Machine
      if (textIndex[4] == 0) editBuffer = "Machine snelheid piek - aanpassing nodig";
      else if (textIndex[4] == 1) editBuffer = "Trust/Sleeve snelheidspieken";
      else editBuffer = "Machine parameters buiten bereik";
      break;
    case 5: // Combinatie
      if (textIndex[5] == 0) editBuffer = "Meerdere stress factoren actief";
      else if (textIndex[5] == 1) editBuffer = "Combinatie van risico signalen";
      else editBuffer = "Complexe stress situatie";
      break;
    case 6: // Onregelmatig
      if (textIndex[6] == 0) editBuffer = "Hartritme onregelmatig - let op";
      else if (textIndex[6] == 1) editBuffer = "Hartslagpatroon afwijkend";
      else editBuffer = "Onregelmatige hartslag detectie";
      break;
    case 7: // Langdurig
      if (textIndex[7] == 0) editBuffer = "Stress periode te lang - onderbreek sessie";
      else if (textIndex[7] == 1) editBuffer = "Langdurige belasting gedetecteerd";
      else editBuffer = "Aanhoudende stress situatie";
      break;
  }
}

EventConfigEvent aiEventConfig_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return ECE_NONE;
  
  uint32_t now = millis();
  if (now - lastTouchMs < CONFIG_COOLDOWN_MS) return ECE_NONE;
  
  // Check edit buttons voor events op huidige pagina
  int startEvent = currentPage * 4;
  for (int i = 0; i < 4; i++) {
    int eventIndex = startEvent + i;
    int yPos = 55 + i * 25;
    
    if (inRect(x, y, 240, yPos-4, 50, 18)) {  // Edit knop
      lastTouchMs = now;
      
      if (editingEvent == eventIndex) {
        // Bevestig edit - save buffer naar config
        strncpy(aiEventConfig.eventNames[eventIndex], editBuffer.c_str(), 63);
        aiEventConfig.eventNames[eventIndex][63] = '\0';
        editingEvent = -1;
        editBuffer = "";
      } else {
        // Start editing this event
        editingEvent = eventIndex;
        editBuffer = String(aiEventConfig.eventNames[eventIndex]);
        // Voor demo: cycle door example texts
        handleTextInput(eventIndex);
      }
      
      aiEventConfig_begin(g);  // Refresh display
      return ECE_NONE;
    }
  }
  
  // Check bottom buttons
  if (inRect(x, y, btnPrevX, btnPrevY, BTN_W, BTN_H) && currentPage > 0) {
    lastTouchMs = now;
    currentPage = 0;
    editingEvent = -1;  // Stop editing when changing pages
    aiEventConfig_begin(g);
    return ECE_NONE;
  }
  
  if (inRect(x, y, btnNextX, btnNextY, BTN_W, BTN_H) && currentPage < 1) {
    lastTouchMs = now;
    currentPage = 1;
    editingEvent = -1;  // Stop editing when changing pages
    aiEventConfig_begin(g);
    return ECE_NONE;
  }
  
  if (inRect(x, y, btnSaveX, btnSaveY, BTN_W, BTN_H)) {
    lastTouchMs = now;
    return ECE_SAVE;
  }
  
  if (inRect(x, y, btnResetX, btnResetY, BTN_W, BTN_H)) {
    lastTouchMs = now;
    return ECE_RESET;
  }
  
  if (inRect(x, y, btnBackX, btnBackY, BTN_W, BTN_H)) {
    lastTouchMs = now;
    return ECE_BACK;
  }
  
  return ECE_NONE;
}