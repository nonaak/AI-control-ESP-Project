# ⚡ QUICK START GUIDE - KEON ESP32

## 🚀 5-MINUTE SETUP

### STAP 1: Hardware
- ✅ ESP32 board
- ✅ USB kabel
- ✅ Keon aangezet

### STAP 2: Software  
1. Download Arduino IDE: https://www.arduino.cc/en/software
2. Install ESP32 support:
   - File → Preferences → Add URL:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Boards Manager → Install "esp32"

### STAP 3: Upload Code
1. Open `Keon_ESP32_Controller.ino`
2. Verander MAC adres (regel 18):
   ```cpp
   #define KEON_MAC_ADDRESS "ac:67:b2:25:42:5a"  // JOUW MAC
   ```
3. Select board: Tools → Board → ESP32 Dev Module
4. Select port: Tools → Port → COM#
5. Click Upload (→)

### STAP 4: Test
1. Open Serial Monitor (115200 baud)
2. Demo draait automatisch!
3. Keon beweegt! 🎉

---

## 📖 FUNCTIES

```cpp
move(position, speed);    // 0-99 voor beide
stop();                   // Stop
moveFast(50);            // Snel naar 50%
```

---

## 🎯 EIGEN CODE

Voeg toe in `loop()`:

```cpp
void loop() {
  // Jouw code hier!
  move(0, 99);     // Down
  delay(500);
  move(99, 99);    // Up
  delay(500);
}
```

---

## ❓ PROBLEMEN?

**"Failed to connect"**
- Koen AAN?
- FeelConnect app GESLOTEN?
- MAC adres CORRECT?

**"Upload error"**
- Juiste COM port?
- USB kabel OK?

---

## 🎉 KLAAR!

Lees `README.md` voor:
- Gedetailleerde instructies
- Advanced features
- Protocol details
- Troubleshooting

**HAVE FUN!** 🚀
