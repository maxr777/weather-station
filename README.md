# Weather Station

ESP32-based weather station built with ESP-IDF, no third-party sensor libraries.

Reads temperature, humidity, and pressure via a BME280 over I2C, monitors air quality with a PMS5003 over UART, and displays everything on an SH1106 OLED. A few diagnostic LEDs signal hardware faults on startup.

All sensor drivers written from scratch against the datasheets. Font by [petabyt](https://github.com/petabyt/font). Startup logo generated with Claude.

**Build:** `idf.py build flash monitor`
