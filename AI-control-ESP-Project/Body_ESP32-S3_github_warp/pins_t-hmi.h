#ifndef PINS_T_HMI_H
#define PINS_T_HMI_H

// LilyGO T-HMI ESP32-S3 Pin Definitions
// 2.8" IPS TFT Display: 240x320 resolution
// ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)

// =============================================================================
// DISPLAY PINS (ST7789V 2.8" IPS TFT)
// =============================================================================
#define TFT_MOSI        11
#define TFT_SCLK        12  
#define TFT_CS          10
#define TFT_DC          14
#define TFT_RST         13
#define TFT_BL          9   // Backlight control

// Display specifications
#define TFT_WIDTH       240
#define TFT_HEIGHT      320
#define TFT_ROTATION    1   // Landscape orientation

// =============================================================================
// TOUCH PINS (CST816S Capacitive Touch)
// =============================================================================
#define TOUCH_SDA       8
#define TOUCH_SCL       48
#define TOUCH_RST       -1  // Connected to TFT_RST
#define TOUCH_INT       16

// =============================================================================
// I2C PINS (Temperature sensor, etc.)
// =============================================================================
#define I2C_SDA         8   // Shared with touch
#define I2C_SCL         48  // Shared with touch

// Alternative I2C pins if needed (GPIO expander available)
#define I2C_SDA_ALT     17
#define I2C_SCL_ALT     18

// =============================================================================
// GPIO EXPANSION (PCF8575 16-bit I2C GPIO expander)
// =============================================================================
#define GPIO_EXP_ADDR   0x20
#define GPIO_EXP_INT    7

// =============================================================================
// VIBRATION MOTOR PINS
// =============================================================================
#define VIBE_PIN        38  // PWM capable pin
#define VIBE_PIN_2      39  // Second vibration motor (optional)

// =============================================================================
// LED PINS
// =============================================================================
#define LED_PIN         21  // Status LED
#define RGB_PIN         -1  // No built-in RGB LED

// =============================================================================
// BUTTON PINS
// =============================================================================
#define BUTTON_1        0   // Boot button (GPIO0)
#define BUTTON_2        47  // Additional button via GPIO expander

// =============================================================================
// AUDIO PINS (Optional I2S audio)
// =============================================================================
#define I2S_BCK         36
#define I2S_WS          37
#define I2S_DOUT        35

// =============================================================================
// SD CARD PINS (if SD card functionality is added)
// =============================================================================
#define SD_CS           -1  // Not available on this board
#define SD_MOSI         -1
#define SD_MISO         -1
#define SD_SCLK         -1

// =============================================================================
// UART PINS (USB CDC available)
// =============================================================================
#define UART_TX         43
#define UART_RX         44

// =============================================================================
// AVAILABLE GPIO PINS FOR SENSORS
// =============================================================================
// Available pins for additional sensors:
// GPIO 1, 2, 3, 4, 5, 6, 15, 17, 18, 19, 20, 41, 42, 45, 46

#define SENSOR_PIN_1    1   // Available for temperature sensor
#define SENSOR_PIN_2    2   // Available for additional sensors
#define SENSOR_PIN_3    3   // Available for additional sensors
#define SENSOR_PIN_4    4   // Available for additional sensors
#define SENSOR_PIN_5    5   // Available for additional sensors

// =============================================================================
// POWER MANAGEMENT
// =============================================================================
#define POWER_EN        -1  // No external power enable pin
#define BATTERY_ADC     -1  // No battery monitoring on this board

// =============================================================================
// HARDWARE SPECIFICATIONS
// =============================================================================
/*
LilyGO T-HMI ESP32-S3 Specifications:
- MCU: ESP32-S3-WROOM-1-N16R8
- Flash: 16MB
- PSRAM: 8MB PSRAM
- CPU: Dual-core Xtensa LX7, up to 240MHz
- Display: 2.8" IPS TFT ST7789V 240x320
- Touch: CST816S Capacitive touch
- GPIO Expander: PCF8575 (16 additional GPIO pins)
- USB: USB-C with CDC support
- Power: 5V USB-C input, 3.3V logic

Key advantages over ESP32-WROOM:
- 4x more Flash memory (16MB vs 4MB)
- 16x more PSRAM (8MB vs 512KB)
- Faster dual-core CPU (240MHz vs 160MHz)
- Larger, higher resolution display (2.8" 320x240 vs 2.4" 240x320)
- Capacitive touch instead of resistive
- USB-C connectivity
- GPIO expansion capability
*/

#endif // PINS_T_HMI_H