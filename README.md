# ⌚ ESP32 Smartwatch POC (M5StickC Plus2)

![Smartwatch Architecture](https://via.placeholder.com/800x400.png?text=ESP32+BLE+Smartwatch+Architecture)

A custom smartwatch Proof of Concept (POC) built on the ESP32 platform. This project features a hardware-accelerated UI, Bluetooth Low Energy (BLE) connectivity, and a companion Android application for time synchronization and data management.

## ✨ Current Features
* **Modern UI Engine:** Silky smooth graphics powered by **LVGL v8.3** with anti-aliased text and custom layouts designed in EEZ Studio.
* **Hardware-Accelerated Display:** Direct DMA rendering to the ST7789 LCD for tear-free, high-performance screen updates.
* **BLE Time Sync:** A custom NimBLE GATT server that accepts UNIX epoch time updates directly from the companion smartphone app.
* **Smart Battery Monitoring:** Custom Exponential Moving Average (EMA) filtering using the ESP32's hardware-calibrated ADC to track the internal battery.

## 🧰 Hardware Requirements
* **Device:** [M5Stack M5StickC Plus2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)
* **MCU:** ESP32-PICO-V3-02 (Dual-core @ 240MHz)
* **Display:** 1.14" ST7789 TFT LCD (240x135)

## 📁 Repository Structure

This repository is a monorepo containing both the embedded firmware and the companion mobile app:

```text
├── esp32_firmware/       # ESP-IDF C/C++ Project
│   ├── main/             # Application code, LVGL setup, and BLE tasks
│   ├── main/ui/          # Generated UI code from EEZ Studio
│   └── CMakeLists.txt
│
└── android_app/          # Android Studio Project (Kotlin/Java)
    ├── app/              # BLE connection and time-sync logic
    └── build.gradle
```

## 🚀 Getting Started

### 1. Building the ESP32 Firmware
This project is built using the official **ESP-IDF** framework.
1. Install [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).
2. Navigate to the firmware directory: `cd esp32_firmware`
3. Set your target: `idf.py set-target esp32`
4. Build and flash the code to your M5StickC Plus2:
   ```bash
   idf.py build flash monitor
   ```

### 2. Building the Android Companion App
1. Open **Android Studio**.
2. Select **File > Open** and choose the `android_app` folder in this repository.
3. Allow Gradle to sync the project dependencies.
4. Build and run the app on a physical Android device (BLE scanning does not work in the Android emulator).

## 🛠️ UI Modification
The graphical interface was designed using **EEZ Studio**. To modify the watch face:
1. Open the EEZ Studio project file.
2. Make your visual changes to the layout or fonts.
3. Click "Generate Code".
4. Copy the newly generated files into `esp32_firmware/main/ui/`.
5. Recompile the firmware.

## 🔮 Next Steps (Roadmap)
- [ ] Implement Screen Timeout / Wake-on-Button to conserve battery.
- [ ] Add Wake-on-Wrist-Turn utilizing the onboard MPU6886 6-axis IMU.
- [ ] Integrate Android Push Notifications (ANCS/NotificationListenerService) to display incoming messages on the watch face.

---
*Built with ESP-IDF, LVGL, and NimBLE.*