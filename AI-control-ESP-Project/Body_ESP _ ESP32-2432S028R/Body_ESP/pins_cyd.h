#pragma once

// ---------- I2C / MAX30102 ----------
#define PIN_SDA       21
#define PIN_SCL       22

// ---------- TFT (CYD ILI9341) --------
static const int PIN_TFT_CS   = 15;
static const int PIN_TFT_DC   = 2;
static const int PIN_TFT_SCK  = 14;
static const int PIN_TFT_MOSI = 13;
static const int PIN_TFT_MISO = 12;
static const int PIN_TFT_RST  = -1;
static const int PIN_TFT_BL   = 21;

// ---------- RGB LED UIT ----------
#define PIN_LED_R 4
#define PIN_LED_G 16
#define PIN_LED_B 17
#define RGB_ACTIVE_LOW 1
