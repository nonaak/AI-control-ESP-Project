# Smart Panlee SC01 Plus - Werkende Implementatie (Zonder PanelLan)

## Inleiding
Deze guide beschrijft hoe je de **Smart Panlee SC01 Plus** (ESP32-S3 met 480x320 IPS display en FT6336U touch controller) kunt gebruiken **zonder de originele PanelLan_esp32 library**, die problemen veroorzaakt met I2C conflicten en onhandige touch mapping.

Deze implementatie gebruikt standaard Arduino libraries en geeft volledige controle over alle hardware componenten.

---

## Hardware Specificaties

### Display
- **Driver:** ST7796 (8-bit parallel interface)
- **Resolutie:** 480x320 pixels (landscape)
- **Type:** IPS display
- **Backlight:** GPIO 45

### Touch Controller
- **Chip:** FT6336U (I2C capacitive touch)
- **I2C Bus:** Wire (GPIO 5 SCL, GPIO 6 SDA)
- **Native resolutie:** 320x480 (portrait)
- **I2C Adres:** 0x38 (standaard voor FT6X36 family)

### I2C Sensoren (Wire1)
- **I2C Bus:** Wire1 (GPIO 11 SCL, GPIO 10 SDA)
- **Componenten:**
  - ADS1115 ADC sensor (0x48)
  - RTC DS3231 (0x68)
  - I2C EEPROM AT24C02 256 bytes (0x50)

---

## Vereiste Libraries

### Core Libraries (Arduino IDE Library Manager)
1. **Arduino_GFX_Library** (by moononournation)
   - Voor ST7796 display driver
   - Snelle 8-bit parallel bus implementatie
   
2. **FT6X36** (by Xylopyrographer)
   - Voor FT6336U touch controller
   - Ondersteunt polling en interrupt mode
   - Flexibele touch event callbacks

3. **Adafruit_ADS1X15**
   - Voor ADS1115 ADC sensor

4. **RTClib** (by Adafruit)
   - Voor DS3231 RTC module

### Library Installatie
```cpp
// Arduino IDE: Tools → Manage Libraries → zoek en installeer:
- Arduino_GFX_Library
- FT6X36
- Adafruit ADS1X15
- RTClib
```

---

## Hardware Pin Configuratie

### Display (8-bit Parallel Bus)
```cpp
#define GFX_BL 45  // Backlight

Arduino_DataBus *bus = new Arduino_ESP32LCD8(
    0,                 // DC
    GFX_NOT_DEFINED,   // CS
    47,                // WR
    GFX_NOT_DEFINED,   // RD
    9, 46, 3, 8, 18, 17, 16, 15  // D0-D7
);

Arduino_GFX *gfx = new Arduino_ST7796(
    bus, 
    4,     // RST
    1,     // rotation (landscape)
    true   // IPS
);
```

### Touch Controller (Wire - I2C0)
```cpp
#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT -1  // Polling mode (geen interrupt pin)

Wire.begin(TOUCH_SDA, TOUCH_SCL);
Wire.setClock(400000);  // 400kHz I2C

FT6X36 ts = FT6X36(&Wire, TOUCH_INT);
ts.begin();
```

### Sensoren (Wire1 - I2C1)
```cpp
#define SENSOR_SDA 10
#define SENSOR_SCL 11

Wire1.begin(SENSOR_SDA, SENSOR_SCL);
Wire1.setClock(400000);

// ADS1115 op adres 0x48
Adafruit_ADS1115 ads;
ads.begin(0x48, &Wire1);

// RTC DS3231 op adres 0x68
RTC_DS3231 rtc;
rtc.begin(&Wire1);

// EEPROM AT24C02 op adres 0x50
#define EEPROM_ADDR 0x50
```

---

## Touch Mapping en Calibratie

### Probleem: Touch Rotatie
De touch controller (FT6336U) rapporteert coordinaten in **portrait mode** (320x480), terwijl het display in **landscape mode** draait (480x320). Dit vereist coordinaat transformatie.

### Oplossing: Flexibele Touch Mapping

#### Methode 1: Rotatie (Eenvoudig)
```cpp
#define TOUCH_ROTATION 1  // 0=0°, 1=90°, 2=180°, 3=270°
#define USE_MANUAL_MAPPING 0  // Gebruik rotatie logica
```

**Rotatie mapping:**
- `0` (0°): Portrait native (x=rawX, y=rawY)
- `1` (90°): Landscape USB rechts (**WERKEND**)
- `2` (180°): Portrait ondersteboven
- `3` (270°): Landscape USB links

**Werkende combinatie voor landscape (USB rechts):**
```cpp
// TOUCH_ROTATION = 1
x = rawY;
y = 320 - 1 - rawX;
```

#### Methode 2: Handmatige Controle (Geavanceerd)
```cpp
#define USE_MANUAL_MAPPING 1
#define MANUAL_SWAP_XY   1   // Wissel X en Y
#define MANUAL_FLIP_X    0   // Spiegel X horizontaal
#define MANUAL_FLIP_Y    1   // Spiegel Y verticaal
```

Deze flags geven volledige controle over de transformatie:
1. **SWAP_XY**: Wissel X en Y coordinaten
2. **FLIP_X**: Spiegel X-as (horizontaal)
3. **FLIP_Y**: Spiegel Y-as (verticaal)

**Transformatie volgorde:**
1. Start met raw coordinaten (portrait)
2. Swap XY (indien enabled)
3. Flip X (indien enabled)
4. Flip Y (indien enabled)
5. Clamp binnen scherm grenzen (0-479 x, 0-319 y)

---

## Touch Event Handling

### Polling Mode (Geen Interrupt)
De FT6336U wordt in polling mode gebruikt zonder interrupt pin. Dit voorkomt extra bedrading en is voldoende voor de meeste toepassingen.

```cpp
void loop() {
  ts.processTouch();  // Lees I2C touch data
  ts.loop();          // Verwerk events en roep callbacks aan
}
```

### Touch Callback met Release Detection
Om spam/herhaalde activaties te voorkomen, detecteren we alleen **release events** (TouchEnd):

```cpp
void touchCallback(TPoint point, TEvent e) {
  // Alleen loslaten verwerken (geen spam)
  if (e != TEvent::TouchEnd) return;
  
  // Raw coordinaten transformeren
  int16_t x, y;
  // ... mapping logica ...
  
  // Actie uitvoeren (bijv. knop activeren)
  checkButtonTouch(x, y);
}
```

**TEvent types:**
- `TouchStart`: Vinger raakt scherm
- `Swipe`: Vinger beweegt over scherm
- `TouchEnd`: Vinger loslaten (**gebruik dit voor knoppen**)

---

## I2C Bus Scheiding

### Waarom Twee I2C Bussen?
De touch controller (FT6336U) en externe sensoren delen **niet dezelfde I2C bus** om conflicten te voorkomen:
- **Wire (I2C0):** Touch controller (GPIO 5/6)
- **Wire1 (I2C1):** ADS1115, RTC, EEPROM (GPIO 10/11)

Deze scheiding voorkomt:
- I2C adres conflicten
- Bus busy errors tijdens touch polling
- Data corruptie bij gelijktijdige reads

### I2C Adressen
**Wire (Touch):**
- 0x38: FT6336U touch controller

**Wire1 (Sensoren):**
- 0x48: ADS1115 ADC
- 0x68: DS3231 RTC
- 0x50: AT24C02 EEPROM (A0/A1/A2 = GND)

---

## EEPROM (AT24C02) - Write/Read/Clear

### Specificaties
- **Type:** AT24C02 I2C EEPROM
- **Capaciteit:** 256 bytes (2 Kbit)
- **I2C Adres:** 0x50 (A0/A1/A2 pins = GND)
- **Page size:** 8 bytes
- **Write cycle:** 5ms

### EEPROM Operaties

#### Write Byte
```cpp
Wire1.beginTransmission(EEPROM_ADDR);
Wire1.write(address);  // Memory address (0-255)
Wire1.write(data);     // Data byte
Wire1.endTransmission();
delay(5);  // Write cycle delay (5ms vereist!)
```

#### Read Byte
```cpp
Wire1.beginTransmission(EEPROM_ADDR);
Wire1.write(address);  // Set read address
Wire1.endTransmission();
Wire1.requestFrom(EEPROM_ADDR, 1);
uint8_t data = Wire1.read();
```

#### Clear Byte (set to 0xFF)
```cpp
Wire1.beginTransmission(EEPROM_ADDR);
Wire1.write(address);
Wire1.write(0xFF);  // Cleared state
Wire1.endTransmission();
delay(5);
```

**BELANGRIJK:** Wacht altijd 5ms tussen writes! EEPROM heeft write cycle tijd nodig.

---

## Display Rendering

### Basis Display Setup
```cpp
void setup() {
  gfx->begin();
  gfx->setRotation(1);  // Landscape
  
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);  // Backlight aan
  
  gfx->fillScreen(0x0000);  // Zwart scherm
}
```

### Kleuren (RGB565 formaat)
```cpp
// Basis kleuren
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
```

### Touch Visualisatie
```cpp
// Teken witte stip op touch positie
gfx->fillCircle(touchX, touchY, 3, COLOR_WHITE);
```

---

## Voorbeeld: Kleur Knoppen met Release Detection

### Knop Structuur
```cpp
struct ColorButton {
  int x, y, w, h;
  uint16_t targetColor;   // Doel kleur bij activatie
  uint16_t currentColor;  // Huidige kleur
  const char* label;
};

ColorButton buttons[] = {
  {20, 30, 100, 70, COLOR_RED, COLOR_WHITE, "ROOD"},
  {360, 50, 100, 70, COLOR_GREEN, COLOR_WHITE, "GROEN"},
  {50, 230, 100, 70, COLOR_BLUE, COLOR_WHITE, "BLAUW"},
  {340, 220, 120, 80, COLOR_YELLOW, COLOR_WHITE, "GEEL"}
};
```

### Knop Detectie op Release
```cpp
bool checkButtonTouch(int x, int y) {
  for (int i = 0; i < 4; i++) {
    if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
        y >= buttons[i].y && y < buttons[i].y + buttons[i].h) {
      // Verander naar target kleur (alleen bij release)
      buttons[i].currentColor = buttons[i].targetColor;
      drawButtons();
      return true;
    }
  }
  return false;
}
```

---

## Complete Testsketch Locatie

De volledige werkende testsketch vind je in:
```
D:\SYSTEEM NONAAK\Documents\Arduino\- warp\Body_ESP32-S3 - Smart Panlee SC01 Plus\test\touch\touch.ino
```

### Wat deze sketch test:
- ✅ ST7796 display initialisatie
- ✅ FT6336U touch controller (polling mode)
- ✅ Touch mapping met rotatie en handmatige controle
- ✅ Touch release detection (geen spam)
- ✅ ADS1115 ADC sensor readout (Wire1)
- ✅ DS3231 RTC tijd en temperatuur (Wire1)
- ✅ AT24C02 EEPROM write/read/clear test
- ✅ Visuele feedback met kleur knoppen
- ✅ Serial debug output (sensor waarden elke 2 sec)

---

## Troubleshooting

### Touch werkt niet
1. **Check I2C bus:** Controleer GPIO 5 (SCL) en GPIO 6 (SDA)
2. **Check I2C adres:** FT6336U = 0x38
3. **Clock speed:** Wire.setClock(400000)
4. **Polling:** Vergeet niet `ts.processTouch()` en `ts.loop()` in loop()

### Touch coordinaten verkeerd
1. **Rotatie mismatch:** Display rotation moet 1 zijn voor landscape
2. **Touch mapping:** Probeer verschillende TOUCH_ROTATION waarden (0-3)
3. **Handmatig:** Zet USE_MANUAL_MAPPING=1 en experimenteer met SWAP/FLIP flags
4. **Debug:** Schakel serial output in callback in om raw vs mapped te zien

### I2C sensoren niet gevonden
1. **Bus conflict:** Gebruik Wire1 voor sensoren, niet Wire
2. **Adressen:** Controleer met I2C scanner (0x48, 0x68, 0x50)
3. **Pull-ups:** Zorg voor externe 4.7kΩ pull-ups op SCL/SDA (Wire1)
4. **Clock speed:** Wire1.setClock(400000) na Wire1.begin()

### EEPROM write/read fails
1. **Write delay:** Wacht altijd 5ms na elke write!
2. **I2C bus:** Gebruik Wire1, niet Wire
3. **Adres pins:** A0/A1/A2 moeten naar GND voor adres 0x50
4. **Endianness:** AT24C02 gebruikt big-endian byte order

### Display blijft zwart/rood
1. **Backlight:** digitalWrite(GFX_BL, HIGH) moet uitgevoerd zijn
2. **Bus init:** Controleer 8-bit parallel bus pin nummers
3. **Rotation:** gfx->setRotation(1) voor landscape
4. **IPS flag:** Laatste parameter in Arduino_ST7796 moet `true` zijn

---

## Belangrijke Verschillen met PanelLan Library

| Aspect | PanelLan_esp32 | Deze Implementatie |
|--------|----------------|-------------------|
| **Touch Library** | Eigen implementatie | FT6X36 (standaard) |
| **Touch Mapping** | Hard-coded rotatie | Flexibel (rotatie + manual) |
| **I2C Conflict** | Ja (touch + sensors op Wire) | Nee (gescheiden bussen) |
| **Touch Mode** | Interrupt only | Polling (geen extra pin) |
| **Release Detection** | Niet ingebouwd | Ja (TEvent::TouchEnd) |
| **Sensor Support** | Beperkt | Volledig (ADS, RTC, EEPROM) |
| **Debug Output** | Minimal | Uitgebreid (serial + visual) |
| **Code Complexiteit** | Hoog (veel dependencies) | Laag (standaard Arduino) |

---

## Samenvatting

Deze implementatie biedt een **stabiele, flexibele en debugbare** basis voor de Smart Panlee SC01 Plus zonder afhankelijkheid van de problematische PanelLan library. 

**Voordelen:**
- ✅ Volledige controle over touch mapping
- ✅ Geen I2C conflicten door bus scheiding
- ✅ Release detection voorkomt knop spam
- ✅ Ondersteuning voor externe I2C sensoren (ADS, RTC, EEPROM)
- ✅ Uitgebreide debug mogelijkheden
- ✅ Standaard Arduino libraries (goed onderhouden)
- ✅ Polling mode (geen interrupt pin nodig)

Upload de testsketch, experimenteer met touch mapping settings, en je hebt een werkend systeem!

---

**Versie:** 1.0  
**Datum:** 2025-10-26  
**Getest op:** Smart Panlee SC01 Plus (ESP32-S3 480x320 IPS)  
**Arduino Core:** arduino-esp32 v2.x / v3.x
