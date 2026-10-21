# Flappy Bird AI on ESP32-S3 (2.8" TFT Touch)

An autonomous, edge-AI Flappy Bird gaming console powered by an **ESP32-S3** microcontroller and a **2.8-inch SPI TFT Touch Display**. The AI agent is trained via **Imitation Learning (DAgger algorithm)** in PyTorch and deployed as a zero-dependency, pure-C neural network running in real-time.

---

## Project Showcase

### Demo Video
<!-- Drop your demo video file into assets/demo_video.mp4 or replace with a YouTube / GIF link -->
<p align="center">
  <video src="assets/demo_video.mp4" controls="controls" width="600" style="max-width: 100%;">
    Your browser does not support the video tag.
  </video>
</p>
<p align="center"><i>Autonomous DAgger AI clearing pipes in real-time on the ESP32-S3 console.</i></p>

### Hardware & Custom Enclosure
<!-- Place your photos in the assets/ directory with matching names:
     - assets/case_front.jpg (Front view showing display & case)
     - assets/case_internal.jpg (Internal wiring / ESP32-S3 mounting)
     - assets/gameplay_action.jpg (Screen close-up during gameplay)
-->
<p align="center">
  <img src="assets/case_front.jpg" alt="Custom Enclosure Front View" width="48%"/>
  <img src="assets/case_internal.jpg" alt="Internal Hardware Assembly" width="48%"/>
</p>
<p align="center">
  <img src="assets/gameplay_action.jpg" alt="Live Gameplay on TFT Screen" width="65%"/>
</p>

---

## What Was Implemented?

1. **TinyML Imitation Learning (DAgger)**:
   - The agent was trained using **DAgger (Dataset Aggregation)** in Python to prevent covariate shift errors.
   - The trained PyTorch model (`8 -> 64 -> 64 -> 2`) is converted into static C arrays stored directly in **Flash ROM**, taking up only ~19.5 KB and zero dynamic heap RAM.
   - Real-time neural forward pass executes in **~80 microseconds** without needing heavy ML frameworks.

2. **Embedded Game Engine**:
   - Built from scratch in pure C to replicate the exact physics, pipe spacing, and state normalization of the desktop PyGame Learning Environment (PLE).

3. **Dual-Core FreeRTOS Architecture**:
   - **Core 0 (PRO_CPU)**: Runs game physics, touch detection, and neural inference at a steady 30 FPS.
   - **Core 1 (APP_CPU)**: Handles high-speed SPI rendering to the 2.8" ILI9341 display via **DMA (Direct Memory Access)** line buffering to prevent screen flicker.

4. **Interactive Touch Modes**:
   - **AI Mode**: The neural network autonomously controls the bird and displays real-time inference latency (`AI FLAP 74US`) on the screen HUD.
   - **Human Mode**: Tap the screen to flap manually and test your own reflexes.
   - **Mode Switch**: Touch and hold the screen for 1.2 seconds to seamlessly switch between Human and AI control.
   - **Quick Restart**: Tap anywhere when the game-over screen appears to instantly reset.

5. **Custom Hardware Enclosure**:
   - A dedicated protective case built to house the ESP32-S3 DevKit, 2.8" touch LCD, breadboard/wiring, and power supply into a clean, portable handheld console.

---

## Hardware & Pin Connections

| 2.8" TFT Pin | ESP32-S3 Pin | Function / Description |
|---|---|---|
| **VCC** | **3V3** (or 5V if module has 3.3V LDO) | Power Supply |
| **GND** | **GND** | Ground |
| **CS** | **GPIO 10** | LCD Chip Select |
| **RESET / RST** | **GPIO 8** | LCD Hardware Reset |
| **DC / RS** | **GPIO 9** | LCD Data / Command Select |
| **MOSI / SDI** | **GPIO 11** | SPI Data (Master Out Slave In) |
| **SCK / CLK** | **GPIO 12** | SPI Clock (40 MHz) |
| **LED / BL** | **GPIO 14** (or 3V3) | Backlight Power |
| **MISO / SDO** | **GPIO 13** | SPI Data (Master In Slave Out) |
| **T_CLK** | **GPIO 12** | Touch Clock (*shares SCK*) |
| **T_CS** | **GPIO 15** | Touch Chip Select |
| **T_DIN / MOSI** | **GPIO 11** | Touch Data In (*shares MOSI*) |
| **T_DO / MISO** | **GPIO 13** | Touch Data Out (*shares MISO*) |
| **T_IRQ** | **GPIO 16** | Touch Interrupt (detects press) |

---

## Code Structure

```text
flappy_dagger_s3/
├── CMakeLists.txt                 # Project build configuration
├── sdkconfig.defaults             # CPU frequency & FreeRTOS settings
├── export_weights.py              # Script to convert .pth weights to C header
├── README.md                      # Project documentation
├── assets/                        # Photos and demo video
└── main/
    ├── CMakeLists.txt             # Source file registration
    ├── model_weights.h            # Static weight arrays (4,866 parameters)
    ├── neural_network.h / .c      # Pure C forward pass (< 0.1 ms latency)
    ├── game_engine.h / .c         # Flappy Bird physics & 8-feature state extractor
    ├── display_driver.h / .c      # ILI9341 SPI driver with DMA line buffering
    ├── touch_driver.h / .c        # XPT2046 touch input & IRQ handling
    └── main.c                     # Dual-core task coordinator & mode switching
```

---

##  Quick Start (Build & Flash)

### 1. Set Up ESP-IDF
Ensure ESP-IDF (v5.1+) is active in your terminal:
```bash
source ~/esp/esp-idf/export.sh
cd flappy_dagger_repo/flappy_dagger_s3
```

### 2. Build the Firmware
```bash
idf.py set-target esp32s3
idf.py build
```

### 3. Flash & Monitor
Connect your ESP32-S3 via USB-C and run:
```bash
# Replace /dev/ttyACM0 with your board's serial port (e.g. /dev/ttyUSB0)
idf.py -p /dev/ttyACM0 flash monitor
```
*(Press `Ctrl + ]` to exit the serial monitor).*
