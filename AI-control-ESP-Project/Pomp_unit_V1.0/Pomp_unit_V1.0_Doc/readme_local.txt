===============================================================================
 Project:   Pomp Unit V1.0 + HoofdESP V1.0
 Auteur:    [Draak]
 Datum:     [vul in]
===============================================================================

INHOUD
------
- Pomp Unit V1.0 (ESP8266)
- HoofdESP Testtool V1.0 (ESP32, serial only)
- HoofdESP UI V1.0 (ESP32, met OLED SH1106)
- Handleiding PDF: Basis_PompUnit_V1.0_Handleiding.pdf

Alle code werkt via ESP-NOW op kanaal 3 met vaste MAC-adressen.

-------------------------------------------------------------------------------
MAC-ADRESSEN
------------
- Pomp Unit (ESP8266):	62:01:94:59:18:86	via ESP-NOW op kanaal 3
- HoofdESP   (ESP32):	E4:65:B8:7A:85:E4	via ESP-NOW op kanaal 4
- M5Atom (ESP32):		50:02:91:87:23:F8	via ESP-NOW op kanaal 2
- Body ES(ESP32):		08:D1:F9:DC:C3:A4	via ESP-NOW op kanaal 1


-------------------------------------------------------------------------------
POMP UNIT V1.0 (ESP8266)
------------------------
Functies:
- Stuurt 2 pompen aan (Vacuum + Lube).
- HX711 uitlezing voor vacuüm (cmHg).
- Servo en lucht-relais gekoppeld aan vacuümpomp:
    Pomp AAN  => servo dicht, lucht dicht
    Pomp UIT  => servo open, lucht open, tare HX711
- Automatische tare met instelbare wachttijd (TARE_DELAY_MS).
- UI: 2 pomp-kaarten, statusdots, cmHg-waarde, wifi-icoon (met kruis bij geen link).
- Lube-pomp: PRIME bij sessiestart, SHOT bij punches of via command.

Pinout:
- HX711: DOUT = D7(GPIO13), SCK = D8(GPIO15)
- Vacuümpomp relais: D5(GPIO14)
- Lube pomp relais:  D6(GPIO12)
- Servo (vacuumklep): D3(GPIO0)
- Lucht-relais: D0(GPIO16)  (LOW=open, HIGH=dicht)
- OLED (I2C SH1106): SDA=D1(GPIO5), SCL=D2(GPIO4)

-------------------------------------------------------------------------------
HOOFDESP TESTTOOL V1.0 (ESP32, Serial only)
-------------------------------------------
- Stuurt V3-payloads via ESP-NOW naar de Pomp Unit.
- Bedien via Serial Monitor (115200 baud).
- Commando’s:
    0 / 1  = arrow_full uit/aan
    a      = force AUTO
    o      = force ON (pomp geforceerd aan)
    f      = force OFF (pomp geforceerd uit)
    s      = session_start pulse → Lube PRIME
    p      = Lube PRIME nu (pulse)
    l      = Lube SHOT nu  (pulse)
    v <x>  = set vacuum setpoint cmHg (bv: v -20.5)
    g <n>  = set punch_goal
    i <n>  = punch_count += n
    x      = verstuur huidig pakket 1x
    b      = toggle beacon 2Hz aan/uit (wifi-icoon)

-------------------------------------------------------------------------------
HOOFDESP UI V1.0 (ESP32 + OLED SH1106)
--------------------------------------
- Zelfde commando’s als testtool (serial).
- Extra grafische UI op OLED 128x64:
    • Linker kaart: Vacuum (setpoint, arrow_full, force-state).
    • Rechter kaart: Lube (punch_count, punch_goal).
    • WiFi-icoon rechtsboven (met kruis bij geen TX).
    • Onderin: Beacon status + laatste TX-resultaat.
- OLED I2C pinout: SDA=21, SCL=22.


-------------------------------------------------------------------------------
TO DO / VOLGENDE STAP
---------------------
- Telemetrie: Pomp Unit moet vacuümwaarden terugzenden naar HoofdESP.
- Nieuwe optie "Zuigen": HoofdESP gebruikt actuele vacuumwaarde.
- Eventueel RX-acknowledge op HoofdESP voor echte link-status.

===============================================================================


