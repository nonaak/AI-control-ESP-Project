# ESP-NOW Communicatie Test Instructies

## Overzicht
De Body ESP is nu geconfigureerd om ESP-NOW berichten te ontvangen van de HoofdESP in plaats van Serial communicatie.

## Netwerk Configuratie
- **Body ESP** (deze unit): `08:D1:F9:DC:C3:A4` - Kanaal 1
- **HoofdESP**: `E4:65:B8:7A:85:E4` - Kanaal 4

## Bericht Structuur
De Body ESP verwacht berichten in deze structuur:
```c
typedef struct {
  float trust;    // Trust speed
  float sleeve;   // Sleeve speed  
  float suction;  // Suction level
  float pause;    // Pause time
  char command[32]; // Optioneel command veld
} esp_now_message_t;
```

## Test Procedure

### 1. Upload en Monitor
1. Upload de Body ESP code
2. Open Serial Monitor (115200 baud)
3. Controleer of ESP-NOW succesvol wordt geïnitialiseerd:
   ```
   ESP-NOW initialized successfully
   ESP-NOW gereed!
   ```

### 2. HoofdESP Configuratie
De HoofdESP moet geconfigureerd worden om berichten te sturen naar:
- **Body ESP MAC**: `08:D1:F9:DC:C3:A4`
- **Kanaal**: 1

### 3. Test Berichten
Wanneer de HoofdESP berichten stuurt, zou je in de Serial Monitor moeten zien:
```
ESP-NOW: T:1.2 S:0.8 Su:50.0 P:2.5
```

### 4. Data Logging Test
1. Start een opname op de Body ESP
2. Controleer dat de CSV bestanden nu extra kolommen bevatten:
   ```
   Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause
   ```
3. Verifieer dat de ESP-NOW data correct wordt gelogd

### 5. Timeout Test
1. Stop berichten van HoofdESP
2. Na de timeout periode (ingesteld in sensor settings) zouden alle waarden op 0 moeten staan

## Troubleshooting

### ESP-NOW Init Failed
- Controleer WiFi kanaal instellingen
- Verifieer dat ESP-NOW library correct is geïnstalleerd

### Geen Berichten Ontvangen
- Controleer MAC adressen in beide ESP's
- Verifieer kanaal configuratie (Body=1, Hoofd=4)
- Controleer of beide ESP's op hetzelfde WiFi kanaal staan

### Data Logging Problemen
- Controleer SD kaart functionaliteit
- Verifieer dat opname actief is
- Controleer CSV bestand formaat

## Debug Output
De Body ESP print debug informatie naar Serial:
- ESP-NOW initialisatie status
- Ontvangen berichten met waarden
- Sensor status berichten

## Volgende Stappen
Na succesvolle ESP-NOW communicatie:
1. Test alle sensoren tegelijkertijd
2. Verifieer complete data logging
3. Test timeout gedrag
4. Controleer prestaties onder load