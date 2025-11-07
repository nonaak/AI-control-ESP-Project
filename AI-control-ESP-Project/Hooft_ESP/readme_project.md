README — Hoofd_ESP (local notes)
=================================

Doel van deze HoofdESP:
Dit is de ESP32 die eigenlijk alles een beetje regelt van de 4 modules
- AtomESP: Die zit op een zuiger/sleeve die op en neer beweegt. Deze ESP32 geeft aan de HoofdESP door welke richting de zuiger/sleeve gaat en hoe snel.
- BodyESP: dat is een esp de de stress probeert te monitoren en kan op een bepaald moment de snelheid van de zuiger/sleeve beïnvloeden, de instellingen kunnen op de bodyESP ingesteld worden daar voor. ik wil daar ook een AI op laten draaien.
- Pomp Unit: daar zitten de pompen in. een vacuüm pomp en een smering pomp. en een vacuüm sensor.
- Alle ESP's kunnen communiceren met elkaar via ESP-NOW.
- In de HoofdESP zit een animatie die erg belangrijk is, die beweegt op en neer, en de snelheid daar van kun je met een Nunchuk besturen. De animatie moet synchroon lopen met wat de AtomESP aangeeft.
- Op HoofdESP kunnen straks ook een Kiirro Keon draadloos op aangesloten worden of de Lovense Solace Pro 2. die kunnen bediend worden door de nunchuk waar dat de animatie op synchroon moet lopen (dat "voelt de AtomESP" dan)
- OP de animatie zit een pijl, als de animatie (dus de zuiger/sleeve) omhoog gaat dat gaat de vacuüm pomp aan en de servo en lucht-relay dicht, als de animatie (dus de zuiger/sleeve) naar beneden gaat dan gaat de vacuüm pomp uit en de servo en lucht-relay open (dat zit in de Pomp Unit).
Tenzij er op er op de Nunchuk de Z knop wordt ingedrukt (aleen als je uit het menu bent) dan gaat de vacuüm pomp aan en de servo en lucht-relay dicht tot de ingestelde waarde (Menu-Instellingen-Zuigen) van de vacuüm sensor in de Pomp Unit.
Als de waarde is bereikt dan gaat de vacuüm pomp uit en de servo en lucht-relay blijven dicht, tot er een 2e keer op Z wordt gedrukt, dan gaat de standaard Vacuüm programma weer werken (dus de Z knop vacuüm overruled het gewone programma).
- In menu is ook een optie Pushes. Daar kun je het aantal "neer" bewegingen van de zuiger/sleeve tellen, als de waarde is bereikt dat gaat de smering-pomp lopen voor een tijd die in te stellen is in menu-lubrication.
- In menu staat ook een optie Start-Lubric. als je daar in menu op drukt met knop Z/enter dat gaat de smering pomp werken voor een bepaalde tijd (om de smering-lijdingen te vullen.

Dit bestand beschrijft hoe je **deze** Arduino-code bouwt, wat de hardware is,
welke libraries je nodig hebt, wat de bediening is, en hoe je snel de 2
belangrijkste visuals aanpast (“Draak” lager, “Instellingen” kleiner).

Deze README hoort bij de *basisversie* die nu werkt en die je mooi vindt
(fonts/kleuren oké, “Draak” lager en “Instellingen” kleiner).


1) Hardware & pinout
--------------------
Board:  ESP32-2432S032C (Jingcai)
LCD:    ST7789, 320x240, **landscape** (ROT=1)
Lib:    Arduino_GFX + Arduino_Canvas

SPI pins in code:
- LCD_DC   = 2
- LCD_CS   = 15
- LCD_SCK  = 14
- LCD_MOSI = 13
- LCD_MISO = GFX_NOT_DEFINED
- LCD_RST  = -1     (geen aparte resetlijn)
- LCD_ROT  = 1      (landscape)
- LCD_BL   = 27     (backlight, **actief hoog**)

I²C (Nunchuk):
- SDA = 21
- SCL = 22

MAC-ADRESSEN
------------
- Pomp Unit (ESP8266):	60:01:94:59:18:86	via ESP-NOW op kanaal 3
- HoofdESP   (ESP32):	E4:65:B8:7A:85:E4	via ESP-NOW op kanaal 4
- M5Atom (ESP32):		50:02:91:87:23:F8	via ESP-NOW op kanaal 2
- Body ES(ESP32):		08:D1:F9:DC:C3:A4	via ESP-NOW op kanaal 1

Let op: Backlight wordt aangezet met `if (LCD_BL >= 0) pinMode(LCD_BL, OUTPUT), HIGH`.
Als je **geen beeld** ziet maar wel compileert/flashed, check eerst of BL (27) hoog staat.


2) Arduino IDE setup
--------------------
- **Board package**: ESP32 by Espressif Systems (2.x of 3.x werkt; je gebruikte al 3.x).
- **Board kiezen**: “ESP32 Dev Module” (of je eigen ESP32 variant die je al gebruikt).
- **Partition scheme**: “Huge APP” is handig, maar “Default” werkt meestal ook.
- **Upload speed/port**: zoals je normaal gebruikt.

Benodigde libraries (Library Manager):
- **Arduino_GFX_Library** (Moononournation)  → bevat ook `canvas/Arduino_Canvas.h`
- **NintendoExtensionCtrl** (voor Nunchuk)
- **Adafruit GFX Library**  → *alleen* nodig voor de **Fonts/…** headers

(De Adafruit-font headers zoals `Fonts/FreeSerifItalic18pt7b.h` komen uit
de Adafruit GFX Library. Zonder die lib krijg je “No such file or directory”.)


3) Projectindeling (tabs)
-------------------------
De code is opgezet in meerdere tabbladen. De namen kunnen licht afwijken, maar
het idee is dit:

- **Hoofd_ESP.ino**  
  Startpunt (setup/loop), include van de overige headers, roept `displayInit()`,
  `displayTick()`, `uiInit()`, `uiTick()` enz.

- **display.h / display.cpp**  
  Alle tekencode voor het linkerpaneel (animatie), speedbar en “Draak”-branding.
  Bevat o.a.:
  - `displayInit()`
  - `displayTick()`
  - `drawSpeedBarTop(step, stepsTotal)`
  - `drawVacArrowHeader(filledUp)`
  - `drawSleeveFixedTop(capY, col)`
  - `drawRodFromCap_Vinside_NoSeam(capY, baseY, rodCol, edgeCol)`  
  - helpers zoals `RGB565u8(...)`

- **ui.h / ui.cpp**  
  Rechter paneel (menu), joystick/Nunchuk-besturing, C- en Z-knoppen, states
  (MENU/ANIM), de popup voor waardes, en de titel “Instellingen”.

- **config.h**  
  Struct `Config` + globale `CFG` met alle kleuren, snelheden, easing, etc.
  Ook de LCD- en I²C-pins, en font-flags zoals `USE_ADAFRUIT_FONTS` en
  `USE_EXOTIC_FONT`.

- **(optioneel) colors.h / storage.h / motion.h / bodyesp.h / m5atom.h**  
  Stubs en hulpmodules. Als een tab niet aanwezig is, negeren — de basis werkt
  zonder extra’s.


4) Bediening (Nunchuk)
----------------------
- **C (kort)**: als hij loopt → *parkeren* naar **beneden** op huidige snelheid, dan pauze;
                als hij pauzeert → **starten**.
- **C (lang)**: wissel **menu-focus** ↔ **animatie-focus** (beide panelen blijven zichtbaar).
- **Z** in **menu**: **ENTER/OK** (toggle **EDIT**); tijdens EDIT verschijnt een popup
  in het **menu-paneel** (rechts) op de geselecteerde regel.
- **Joystick Y**:
  - In **animatie-focus**: verander **speed step** (0..7).  
  - In **menu**: scrol/selecteer; in EDIT: waarde verhogen/verlagen.
- De **speedbar** boven het linkerframe toont de huidige step.
- De **VAC-pijl** (links, header) vult tijdens opwaartse beweging (vacuum).


5) Snel aanpassen (alleen deze 2 dingen)
----------------------------------------
A) **“Draak” lager**  
   In **display.cpp**, functie `drawBrandingDraak()`:
   - Zoek: `int desiredTy = barY - 14;`
   - **Grotere** (absolute) waarde → **lager**. Voorbeeld:
     - `barY - 14`  (nu) → iets lager
     - `barY - 18`  → nog lager

B) **“Instellingen” kleiner**  
   In **ui.cpp**, functie `setMenuTitleAndItems()` (de titel-kop boven het rechter paneel):
   - We gebruiken de **kleinere** item-font voor titels (of `setTextSize(1)`).
   - Wil je nóg kleiner/groter? Pas hier de gekozen font of `setTextSize` aan.


6) Bouwen & flashen
-------------------
1. Open de `.ino` in Arduino IDE.
2. Kies het juiste ESP32 board + COM-poort.
3. Controleer dat de libraries zijn geïnstalleerd (zie §2).
4. Compileer & upload.

Tip: Als je na upload **zwart beeld** krijgt maar de code draait:
- Controleer **LCD_BL = 27** en dat die op **HIGH** gezet wordt.
- Laat `LCD_ROT = 1` staan (landscape).
- ST7789 wiring & voeding oké?


7) Troubleshooting (kort en praktisch)
--------------------------------------
- **Fonts/FreeSerifItalic18pt7b.h: No such file or directory**  
  → Installeer **Adafruit GFX Library** (de font headers komen daar vandaan).

- **undefined reference** naar `drawSleeveFixedTop(...)` of `drawRodFromCap_...`  
  → `display.cpp` moet gecompileerd worden **en** `display.h` moet in `ui.cpp`
  worden **geïncludeerd** (prototypes).

- **'RGB565u8' not declared**  
  → `RGB565u8` staat in `display.h`. Include `display.h` waar je die helper gebruikt.
  Eventueel kun je deze helper naar een eigen `utils.h` verplaatsen en overal includen.

- **#include nested depth / circular include**  
  → Zorg voor **include guards** in headers (`#pragma once`) en **geen** wederzijdse
  `#include "ui.h"` ↔ `#include "ui.h"` lussen. Laat `.cpp`-bestanden alleen de
  **nodige** headers includen.

- **Geen beeld / backlight uit**  
  → Check dat `LCD_BL` bestaat en **HIGH** gezet wordt (pin 27).


8) Kleuren, fonts en stijl
--------------------------
- Kleuren vind je in `CFG` (struct in `config.h`), bijv. `CFG.COL_BG`, `CFG.COL_MENU_PINK`,
  `CFG.COL_BRAND` en de rod-kleuren (`rodSlow...` → `rodFast...`).
- Fonts: via `USE_ADAFRUIT_FONTS` en `USE_EXOTIC_FONT`. Als je `USE_EXOTIC_FONT 1` gebruikt
  en de Adafruit GFX Library hebt, kun je o.a. `FreeSerifItalic...` inzetten.
- De canvas-afmetingen voor de animatie staan in `display.cpp` gedefinieerd
  (L_CANVAS_W/H/X/Y).

9) Versielog (lokaal)
---------------------
- **Baseline (deze)**: *“Draak” lager, “Instellingen” kleiner*, backlight-guard.
  Dit is de versie die we als **basis** behouden.

10) Tip
-------
Sla dit hele mapje met tabs + deze `readme_local.txt` op in Drive. Als je later
wijzigingen wil, begin met deze basis en vraag alleen om de **complete tab(s)**
met de **kleine** aanpassingen — zo blijft alles consistent.

  📘 Hoofd_ESP V3 – Werkende Basis (2025-09-11)

## ✅ Inbegrepen
- Volledige werkende tabbladen
- Dummy-implementaties voor ontbrekende modules (`motion.*`, `colors.h`, `m5atom.h`, enz.)
- Alle `CFG.COL_*`, `u8_to_RGB565`, `rgb2hsv_u8`, enz. gefixt
- Geen compileerfouten onder ESP32 Arduino Core v3.x

## 📌 Afspraken
- Bij "maak het nu werkend" mag ChatGPT zelf dummy-tabbladen aanvullen
- Alle bestanden blijven volledig tabblad-per-tabblad leverbaar
- Bij "truste" wordt alles verwerkt in ZIP en geüpload

Laatste actie: project compileert zonder fouten. Klaar voor volgende iteratie.

(Deze map is Hooft_ESP/)
— Einde —
