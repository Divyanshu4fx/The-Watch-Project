# ESP32 Smartwatch POC (M5StickC Plus2)

![Smartwatch Image](https://github.com/user-attachments/assets/29b40b3b-25c6-4d8f-a2c6-81de90ecb267)

A functional smartwatch proof of concept built on the ESP32. It uses a BLE GATT server to receive time updates from a custom Android companion app.

## Features
* **UI:** LVGL v8.3, generated via EEZ Studio.
* **Display:** ST7789 TFT LCD using the ESP-IDF `esp_lcd` driver with DMA.
* **BLE:** NimBLE stack configured to sync UNIX time from Android.
* **Battery:** GPIO38 ADC reading with hardware calibration and EMA software filtering.

## Hardware
* **Device:** [M5Stack M5StickC Plus2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)
* **MCU:** ESP32-PICO-V3-02

## Repository Structure
```text
├── smart_watch/       # ESP-IDF C/C++ project
│   ├── main/             # Core logic and BLE tasks
│   ├── main/ui/          # Generated EEZ Studio files
│   └── CMakeLists.txt
└── WatchAppNordic/          # Android Studio project
    ├── app/              # BLE connection and time sync
    └── build.gradle
```

## Build Instructions

### 1. ESP32 Firmware
Requires **ESP-IDF v5.x**.
```bash
cd smart_watch
idf.py set-target esp32
idf.py build flash monitor
```

### 2. Android App
1. Open the `WatchAppNordic` folder in Android Studio.
2. Sync Gradle and build.
3. Run on a physical Android device (BLE scanning fails in the emulator).

## Editing the UI
1. Open the UI project in EEZ Studio.
2. Edit the layout and click "Generate Code".
3. Overwrite the files in `smart_watch/main/ui/` with the new export.
4. Rebuild the ESP32 firmware.

## To-Do
- [ ] Wake-on-wrist-turn (using the MPU6886 IMU).
