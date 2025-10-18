Board: ESP32S3 Dev Module

Configuration:
•  USB CDC On Boot: Enabled
•  CPU Frequency: 240MHz (WiFi/BT)
•  Flash Mode: QIO 
   USB Mode: Kies "Hardware CDC and JTAG" 
•  Flash Size: 16MB
•  Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS) of Huge APP (3MB No OTA/1MB SPIFFS)
•  PSRAM: OPI PSRAM
   Upload Mode: "UART0 / Hardware CDC" of "USB-OTG CDC"

Voor jouw toepassing met SD card recording en veel data zou ik aanraden:
•  Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)

Dit geeft je:
•  3MB voor de applicatie code
•  9.9MB FATFS voor data opslag (als backup voor SD kaart)

Of als je meer app space nodig hebt:
•  Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)

Upload Settings:
•  Upload Speed: 921600 (of 115200 als problemen)
•  Arduino Runs On: Core 1
•  Events Run On: Core 1

Libraries die je nodig hebt:
•  TFT_eSPI (voor T-HMI display)
•  Adafruit ADS1X15 (voor nieuwe ADS1115 sensoren)
•  ESP32 core libraries (SD_MMC, WiFi, Wire, etc.)

Deze instellingen zijn geoptimaliseerd voor de T-HMI hardware met 16MB flash en OPI PSRAM voor de beste prestaties.

ADS1115 = 0x48
