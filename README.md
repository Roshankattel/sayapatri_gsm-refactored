# Sayapatri GSM — ESP32 Cellular POS Terminal Firmware

A complete Point-of-Sale (POS) terminal firmware for an **ESP32 WROVER** device that combines a **SIM800 cellular modem**, **PN532 NFC reader**, **ILI9341 TFT display**, **4×3 matrix keypad**, and **buzzer** into a standalone payment and recharge terminal. The device operates entirely over **2G GPRS cellular data** — no Wi-Fi required — and communicates with a cloud backend over **HTTPS** to process NFC card-based transactions.

The Sayapatri brand logo and UI are displayed on the TFT screen throughout the user experience.

---

## Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [How It Works — End-to-End Flow](#how-it-works--end-to-end-flow)
- [Hardware Requirements](#hardware-requirements)
- [Pin Configuration](#pin-configuration)
  - [GSM Modem (SIM800)](#gsm-modem-sim800)
  - [NFC Reader (PN532)](#nfc-reader-pn532)
  - [TFT Display (ILI9341)](#tft-display-ili9341)
  - [Keypad](#keypad)
  - [Buzzer](#buzzer)
- [Software Architecture](#software-architecture)
  - [Layered Stack](#layered-stack)
  - [Module Responsibilities](#module-responsibilities)
  - [FreeRTOS Usage](#freertos-usage)
  - [PSRAM Usage](#psram-usage)
- [Project Structure](#project-structure)
- [Software Dependencies](#software-dependencies)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [Configuration Reference](#configuration-reference)
  - [Known Build Issues and Fixes](#known-build-issues-and-fixes)
  - [Building and Flashing](#building-and-flashing)
  - [Serial Monitoring](#serial-monitoring)
- [Application Flow in Detail](#application-flow-in-detail)
  - [Boot Sequence](#boot-sequence)
  - [State Machine](#state-machine)
    - [WELCOME State](#welcome-state)
    - [AMOUNT State](#amount-state)
    - [SCAN State](#scan-state)
    - [NOTIFY State](#notify-state)
- [NFC Card Processing](#nfc-card-processing)
  - [Card Detection](#card-detection)
  - [UID to Hash Conversion](#uid-to-hash-conversion)
- [Buzzer Behavior](#buzzer-behavior)
- [Display System](#display-system)
  - [Screen Pages](#screen-pages)
  - [Embedded Image Assets](#embedded-image-assets)
  - [Status Icons](#status-icons)
- [API Reference](#api-reference)
  - [Authentication — Login](#authentication--login)
  - [Transaction Submission](#transaction-submission)
- [Test Mode](#test-mode)
- [Debug Mode](#debug-mode)
- [Security Notes](#security-notes)
- [Troubleshooting](#troubleshooting)
- [Extending the Firmware](#extending-the-firmware)

---

## Overview

The device is a **merchant-operated POS kiosk**. The merchant uses the keypad to choose the transaction type and amount; the customer taps their **ISO 14443A NFC/RFID card** to authorize. The firmware hashes the card UID with **SHA-256**, logs in to the backend with merchant credentials over cellular, and submits the transaction. The result — the customer name and success/failure — is shown on the TFT screen and signalled with a buzzer pattern.

**Key characteristics:**

- Cellular-only: works anywhere with 2G coverage. No Wi-Fi or Ethernet required.
- Tamper-resistant: raw card UIDs never leave the device; only the SHA-256 digest is transmitted.
- Self-recovering: GPRS reconnect retry on boot; automatic screen return after a transaction.
- PSRAM-backed JSON: large ArduinoJson documents are allocated in SPI RAM, freeing the heap.
- Runs on **FreeRTOS** alongside the Arduino loop for smooth display during modem init.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      ESP32 WROVER Module                         │
│                                                                  │
│  ┌─────────────────┐   ┌──────────────────┐   ┌──────────────┐  │
│  │  4×3 Keypad     │   │  PN532 NFC/RFID  │   │ ILI9341 TFT  │  │
│  │  GPIO Matrix    │   │  (I2C + IRQ pin) │   │  SPI 240×320 │  │
│  │  Rows: 34,36,   │   │  ISO 14443A read │   │  Rotation: 1 │  │
│  │  39, 35         │   │  IRQ → GPIO 13   │   │  Landscape   │  │
│  │  Cols: 32,14,33 │   │  RST → GPIO 25   │   │              │  │
│  └─────────────────┘   └──────────────────┘   └──────────────┘  │
│                                                                  │
│  ┌──────────────┐   ┌────────────────────────────────────────┐  │
│  │  Buzzer      │   │  SIM800 GSM/GPRS Modem                 │  │
│  │  GPIO 12     │   │  UART (Serial1): TX=27, RX=26          │  │
│  │  Active HIGH │   │  PWRKEY=4, POWER_ON=23, RST=5          │  │
│  └──────────────┘   │  APN: "web" (configurable)             │  │
│                      └────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Software Stack                                          │   │
│  │  Arduino Loop → State Machine → TinyGSM → SSLClient     │   │
│  │  → ArduinoHttpClient → HTTPS POST → JSON (PSRAM)        │   │
│  └──────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
                               │
                            2G GPRS
                               │
                  ┌────────────────────────┐
                  │   rfid.test.com     │
                  │   HTTPS port 443       │
                  │                        │
                  │  POST /api/v1/auth/login
                  │  POST /api/v1/transaction/add
                  └────────────────────────┘
```

---

## How It Works — End-to-End Flow

1. **Power on** — the buzzer initializes, the TFT shows the Sayapatri logo with a 27-frame animated loading spinner (40 ms/frame), then the PN532 NFC reader is initialized over I2C.
2. **Modem power-up** — the SIM800 is brought up via a `PWRKEY` pulse sequence per the SIM800 AT manual. `SerialAT` (UART at 115 200 baud) is started. TinyGSM calls `modem.restart()` until the modem responds.
3. **GPRS connect** — up to 3 attempts to call `modem.gprsConnect(APN, ...)`. If all fail the screen shows "Network Unavailable" and the device restarts after 40 seconds.
4. **Home page** — after a successful GPRS connection the home page renders (Sayapatri logo, battery icon, cellular signal icon) and the welcome screen prompts for `1. Recharge` or `2. Payment`.
5. **Transaction** — keypad drives a 4-state machine: `WELCOME → AMOUNT → SCAN → NOTIFY`.
6. **Card scan** — PN532 IRQ pin edge triggers the card read. The UID bytes are assembled into a string (reversed, integer-formatted), then SHA-256 hashed via **mbedtls** into a 64-char hex digest.
7. **HTTPS calls** — merchant login (JWT), then transaction submission with card hash + amount + type.
8. **Result** — customer name (success) or error message shown on screen; buzzer plays SUCCESS or ERROR pattern. After 10 seconds the display resets to the welcome screen.

---

## Hardware Requirements

| Component | Recommended Part | Notes |
|-----------|-----------------|-------|
| MCU Board | **TTGO T-Call V1.3 or V1.4** (ESP32 WROVER + SIM800) | Pin definitions in code match this board. A bare ESP32 WROVER + external SIM800 also works if wired to the same GPIOs. |
| NFC Reader | **Adafruit PN532 Breakout** | Must be in **I2C mode** (configure DIP switches/solder jumpers on the breakout: A0=1, A1=0) |
| Display | **ILI9341 2.4" SPI TFT** (240×320) | Many cheap breakout boards work. Ensure 3.3 V logic level. |
| Keypad | **4×3 Matrix Keypad** (12 keys) | Standard membrane or mechanical, 7-pin connector |
| Buzzer | Any 3.3 V / 5 V active or passive buzzer | GPIO 12 → buzzer → GND. Active buzzers are simpler (no PWM needed). |
| SIM Card | Any 2G GPRS-capable SIM with a data plan | Confirm 2G coverage in your deployment region. Many carriers are sunsetting 2G — verify before deploying. |
| Power | 3.7 V LiPo battery or regulated supply | The SIM800 draws up to **2 A peak** during TX bursts. Use a supply or battery with sufficient current capacity. Inadequate power causes random resets. |

> **TTGO T-Call note:** Versions V1.3 and V1.4 differ in the modem UART pin assignments. The code matches the **V1.3** pinout (`TX=27, RX=26`). Check the silkscreen on your board.

---

## Pin Configuration

### GSM Modem (SIM800)

These match the **TTGO T-Call V1.3** pinout. Defined in `include/main.h`.

| Signal | GPIO | Description |
|--------|------|-------------|
| `MODEM_PWRKEY` | **4** | Pulse LOW→HIGH for >1 s to power on/off |
| `MODEM_RST` | **5** | Hard reset line (active LOW) |
| `MODEM_POWER_ON` | **23** | Drive HIGH to enable power regulator |
| `MODEM_TX` | **27** | ESP32 TX → Modem RX |
| `MODEM_RX` | **26** | Modem TX → ESP32 RX |

**Power-on sequence (from `ttgoGsmSetup()`):**

```
MODEM_POWER_ON → HIGH          (enable regulator)
MODEM_PWRKEY   → HIGH          (start high)
delay(100 ms)
MODEM_PWRKEY   → LOW           (pull low)
delay(1000 ms)                 (hold >1 s per SIM800 datasheet)
MODEM_PWRKEY   → HIGH          (release)
SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX)
modem.restart()                (send AT commands until OK)
```

### NFC Reader (PN532)

Defined in `include/config.h`. The Adafruit PN532 library is initialized in I2C mode.

| Signal | GPIO | Description |
|--------|------|-------------|
| `PN532_IRQ` | **13** | Active-LOW interrupt from PN532 when a card is present |
| `PN532_RESET` | **25** | PN532 hardware reset (active LOW) |
| SDA | **21** | Default ESP32 Arduino I2C data |
| SCL | **22** | Default ESP32 Arduino I2C clock |

> On the PN532 breakout board, set **I2C mode** by configuring the interface selection jumpers: SEL0 = HIGH (1), SEL1 = LOW (0).

### TFT Display (ILI9341)

Configured in `lib/TFT_eSPI/User_Setup.h`. The driver is set to `ILI9341_DRIVER`.

| Signal | GPIO | Description |
|--------|------|-------------|
| `TFT_MOSI` | **19** | SPI data from ESP32 to display |
| `TFT_SCLK` | **18** | SPI clock |
| `TFT_CS` | **15** | Chip select (active LOW) |
| `TFT_DC` | **2** | Data/Command select |
| `TFT_RST` | — | Not connected; tie to 3.3 V or EN pin |
| `TFT_MISO` | — | Not used (display is write-only in this firmware) |

The display is initialized in **landscape rotation (rotation = 1)**, giving 320×240 pixels. Text is rendered with font index `GFXFF` (the built-in GFX free font family, size 4).

### Keypad

Defined in `include/main.h`. Standard 4-row × 3-column matrix.

```
Key layout:
  [ 1 ] [ 2 ] [ 3 ]   Rows: GPIOs 34, 36, 39, 35
  [ 4 ] [ 5 ] [ 6 ]   Cols: GPIOs 32, 14, 33
  [ 7 ] [ 8 ] [ 9 ]
  [ * ] [ 0 ] [ # ]
```

| Function | Key |
|----------|-----|
| Select Recharge | `1` |
| Select Payment | `2` |
| Enter digit | `0`–`9` |
| Backspace (delete last digit) | `*` |
| Confirm amount | `#` |

### Buzzer

Defined in `include/buzz.h`.

| Signal | GPIO |
|--------|------|
| `BUZZER_PIN` | **12** |

Drive HIGH to activate. Patterns are non-blocking (timed in the main loop via `checkBuzzerStatus()`).

---

## Software Architecture

### Layered Stack

```
Application Logic (main.cpp, welcome/amount/scan/notify.cpp)
        │
        ▼
  DisplayFunction lib  ──── TFT_eSPI (SPI driver) ───► ILI9341
        │
        ▼
  ArduinoHttpClient ─────── SSLClient (TLS/mbedtls)
        │                        │
        ▼                        ▼
  TinyGsmClient ─────────── SIM800 (AT commands via Serial1)
        │
        ▼
    GPRS / 2G Network ──────────────────────────────► rfid.test.com:443
```

### Module Responsibilities

| File | Responsibility |
|------|---------------|
| `src/main.cpp` | `setup()` / `loop()`, modem power-on sequence, GPRS connect with retry, FreeRTOS processing task, state dispatch |
| `src/welcome.cpp` | Reads keypad, sets `rechargeRequest` flag, transitions to AMOUNT |
| `src/amount.cpp` | Numeric keypad entry with overflow guard, backspace, confirm; stores result in global `amount` |
| `src/scan.cpp` | Polls PN532 IRQ, reads UID, derives SHA-256 hash, calls `merchantLogin()` / `transactionRequest()`, handles HTTP error codes |
| `src/httpsReq.cpp` | Builds and sends HTTPS POST requests; `merchantLogin()` returns JWT; `transactionRequest()` returns customer name |
| `src/notify.cpp` | Reads `httpCode` and `userName`, renders result on TFT, triggers buzzer |
| `src/buzz.cpp` | Non-blocking buzzer driver; `playBuzz()` starts a pattern; `checkBuzzerStatus()` manages timing in the loop |
| `src/utils.cpp` | `getHash()` — SHA-256 via mbedtls with PSRAM alloc; `lcdDisplay()` — two-line TFT text helper; `notifyTimer()` — auto-return to welcome after timeout |
| `lib/DisplayFunction/` | All TFT rendering: init, pages (home, welcome, amount, scan), PNG image decode, loading animation, icon overlays |
| `include/config.h` | Compile-time constants: server host/port, PN532 pins, APN, limits, `DEBUG`, `TEST` flags, enums |
| `include/main.h` | Global state variables, keypad map/pins, modem pins, NFC instance, TinyGSM modem instance, merchant credentials |
| `include/gsmSetup.h` | Configures `TINY_GSM_MODEM_SIM800`, `SerialAT = Serial1`, RX buffer size; includes TinyGSM/SSLClient/HttpClient |
| `include/psram.h` | `SpiRamAllocator` struct + `SpiRamJsonDocument` typedef — ArduinoJson backed by PSRAM via `heap_caps_malloc` |

### FreeRTOS Usage

During the boot sequence (modem init + GPRS connect), which can take 10–60 seconds, a **FreeRTOS task** runs the loading animation on core 1:

```cpp
xTaskCreatePinnedToCore(
    processDisplay,        // Loops showProcessing() every 1 s
    "Processing Display",
    100000,                // 100 KB stack (large because of TFT rendering)
    NULL,
    1,                     // Priority 1
    &display_handle,
    CONFIG_ARDUINO_EVENT_RUNNING_CORE
);
```

Once GPRS connects (or fails), the task is deleted with `vTaskDelete(display_handle)` and the UI transitions to the main screen. This keeps the display responsive while the blocking modem AT-command sequence runs.

### PSRAM Usage

The ESP32 WROVER has **4 MB of SPI RAM (PSRAM)** in addition to the 520 KB internal SRAM. ArduinoJson `DynamicJsonDocument` on the heap can cause fragmentation with large payloads. This firmware uses a custom `SpiRamAllocator` (`include/psram.h`) that routes all JSON document allocations to PSRAM via `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. This is critical because:

- The login response JSON and transaction response JSON can each exceed 1 KB.
- TFT rendering and the TinyGSM RX buffer (1030 bytes) already consume significant heap.

The `SpiRamJsonDocument` is a drop-in replacement for `DynamicJsonDocument`:

```cpp
SpiRamJsonDocument doc(4096);   // 4 KB allocated from PSRAM
doc["email"] = MERCHANT_EMAIL;
```

---

## Project Structure

```
sayapatri_gsm-refactored/
│
├── platformio.ini                  # Build environment, board, libs, upload settings
│
├── include/                        # Application-wide headers
│   ├── config.h                    # Server address, pins, APN, limits, enums, DEBUG/TEST
│   ├── main.h                      # Global vars, keypad layout & pins, modem pins,
│   │                               #   NFC instance, modem instance, merchant creds
│   ├── gsmSetup.h                  # TinyGSM modem type, SerialAT, RX buffer size
│   ├── httpReq.h                   # Declarations: sendHttpPostRequest, merchantLogin,
│   │                               #   transactionRequest
│   ├── psram.h                     # SpiRamAllocator + SpiRamJsonDocument typedef
│   ├── utils.h                     # lcdDisplay, notifyTimer, getHash declarations
│   ├── buzz.h                      # BUZZER_PIN, playBuzz, checkBuzzerStatus
│   ├── welcome.h                   # welcomeProcess declaration
│   ├── amount.h                    # amountProcess declaration
│   ├── scan.h                      # scanProcess, startListeningToNFC declarations
│   └── notify.h                    # notifyProcess declaration
│
├── src/                            # Application source files
│   ├── main.cpp                    # setup(), loop(), GSM power-on, GPRS connect,
│   │                               #   FreeRTOS display task
│   ├── welcome.cpp                 # Keypad poll for 1/2 selection
│   ├── amount.cpp                  # Keypad numeric entry with backspace and limit
│   ├── scan.cpp                    # NFC IRQ detection, UID read, SHA-256, HTTPS flow
│   ├── httpsReq.cpp                # merchantLogin(), transactionRequest(), HTTP POST
│   ├── notify.cpp                  # Display success/error, trigger buzzer
│   ├── buzz.cpp                    # playBuzz(), checkBuzzerStatus() timed patterns
│   └── utils.cpp                   # getHash() SHA-256, lcdDisplay(), notifyTimer()
│
├── lib/
│   ├── DisplayFunction/            # Project-specific UI library
│   │   ├── displayFunction.h       # TFT API declarations, image coordinates as #defines
│   │   ├── displayFunction.cpp     # TFT_eSPI init, PNG decode, all screen page functions
│   │   └── images/                 # Embedded image headers (PNG bytes as C arrays)
│   │       ├── logo.h              # Sayapatri brand logo
│   │       ├── battery.h           # Battery level icon
│   │       ├── cellular.h          # Cellular signal icon
│   │       ├── charging.h          # Charging indicator icon
│   │       ├── bluetooth.h         # Bluetooth icon (reserved for future use)
│   │       ├── diamond.h           # Pair/connection indicator diamonds
│   │       └── startAnimation.h    # 27-frame loading animation (raw pixel arrays)
│   │
│   ├── SSLClient/                  # Vendored TLS wrapper (mbedtls-based)
│   │   └── src/
│   │       ├── SSLClient.h/.cpp    # SSLClient class wrapping any Arduino Client
│   │       └── ssl_client.h/.cpp   # Low-level mbedtls handshake
│   │
│   └── TFT_eSPI/                   # Full Bodmer TFT_eSPI driver library
│       ├── User_Setup.h            # *** EDIT THIS *** — driver, SPI pins for ILI9341
│       ├── User_Setup_Select.h     # Points to User_Setup.h
│       └── ...                     # Driver sources, fonts, examples
│
└── test/
    └── README                      # PlatformIO unit test placeholder (no tests yet)
```

---

## Software Dependencies

### PlatformIO Library Registry (`lib_deps` in `platformio.ini`)

| Library | Version | Purpose |
|---------|---------|---------|
| `adafruit/Adafruit PN532` | `^1.2.3` | High-level I2C/SPI driver for the PN532 NFC chip; `readDetectedPassiveTargetID()`, `SAMConfig()`, `getFirmwareVersion()` |
| `chris--a/Keypad` | `^3.1.1` | Non-blocking matrix keypad scan; `kpd.getKey()` returns the pressed key character or 0 |
| `bitbank2/PNGdec` | `^1.0.1` | Streaming PNG decoder; used to draw brand images and icons stored as C byte arrays in flash |
| `bblanchon/ArduinoJson` | `^6.19.3` | JSON serialization and deserialization; `SpiRamJsonDocument` (PSRAM-backed) used throughout |
| `vshymanskyy/TinyGSM` | `^0.11.5` | AT-command abstraction for SIM800; provides `TinyGsmClient` which is a standard Arduino `Client` |
| `arduino-libraries/ArduinoHttpClient` | `^0.4.0` | HTTP/HTTPS client on top of any `Client` transport; handles headers, body, status code |

### Vendored Libraries (`lib/` directory)

| Library | Origin | Purpose |
|---------|--------|---------|
| **TFT_eSPI** | Bodmer/TFT_eSPI | SPI display driver for ILI9341; hardware-accelerated, DMA-capable, supports custom fonts and PNG rendering |
| **SSLClient** | govorox/SSLClient (modified) | Wraps a `TinyGsmClient` with a mbedtls TLS session so `ArduinoHttpClient` can make HTTPS calls over GPRS. The `govorox/SSLClient@^1.0.0` PlatformIO registry entry is commented out in `platformio.ini` in favour of this local copy. |
| **DisplayFunction** | Project-specific | All screen page rendering, PNG image helpers, and loading animation. Separates display logic from business logic. |

### Implicit ESP-IDF / Arduino Components

| Component | Usage |
|-----------|-------|
| `mbedtls` (ESP-IDF built-in) | SHA-256 hashing in `getHash()` via `mbedtls_md_context_t`. Also used internally by SSLClient for TLS. |
| `FreeRTOS` (Arduino ESP32) | Background processing animation task during modem init (`xTaskCreatePinnedToCore`, `vTaskDelete`) |
| `heap_caps_malloc` / `MALLOC_CAP_SPIRAM` | PSRAM allocations for JSON documents and SHA-256 buffers (`ps_malloc`) |
| `Wire` (Arduino I2C) | PN532 I2C communication |

---

## Getting Started

### Prerequisites

1. **[PlatformIO](https://platformio.org/)** — Install via the VS Code extension (recommended) or CLI:
   ```bash
   pip install platformio
   ```
2. **USB-to-serial driver** — install the driver for your board's USB chip:
   - **CP2102** (most TTGO T-Call boards): [Silicon Labs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
   - **CH340**: [CH340 driver](http://www.wch.cn/downloads/CH341SER_EXE.html)
3. **A SIM card** with an active **2G/GPRS data plan**. Insert it into the SIM800 SIM tray with the device powered off.
4. **Python 3** (used by PlatformIO build scripts).

### Installation

```bash
git clone https://github.com/<your-org>/sayapatri_gsm-refactored.git
cd sayapatri_gsm-refactored
```

PlatformIO will automatically download all `lib_deps` on first build.

### Configuration Reference

All configuration lives in two files. Edit them before building.

#### `include/config.h` — Application constants

```cpp
#define DEBUG 1       // 1 = enable Serial debug output, 0 = silent
#define TEST  0       // 1 = skip real NFC, use dummy hash (see Test Mode section)

// Transaction limit — amounts above this are rejected at entry
const int AMOUNT_LIMIT = 1000000;   // 1,000,000 (one lakh)

// Notify screen auto-dismiss timeout
const int NOTIFY_TIME = 10000;      // milliseconds (10 seconds)

// Backend server
const char hostname[] = "rfid.test.com";
const int  port       = 443;

// NFC reader GPIO pins
const uint8_t PN532_IRQ   = 13;
const uint8_t PN532_RESET = 25;

// GPRS — change APN to match your SIM card's carrier settings
const char GPRS_APN[16]      = "web";   // e.g. "airtelgprs.com", "internet", etc.
const char GPRS_USERNAME[16] = "";      // Usually empty
const char GPRS_PASSWORD[16] = "";      // Usually empty
const char GPRS_SIM_PIN[16]  = "";      // SIM PIN, if your SIM requires one
```

#### `include/main.h` — Merchant credentials and pin definitions

```cpp
// *** REPLACE THESE with real credentials before deployment ***
String MERCHANT_EMAIL    = "ajay@gmail.com";
String MERCHANT_PASSWORD = "Hello123@";

// Modem GPIO pins — match your board revision
const uint8_t MODEM_RST      = 5;
const uint8_t MODEM_PWRKEY   = 4;
const uint8_t MODEM_POWER_ON = 23;
const uint8_t MODEM_TX       = 27;
const uint8_t MODEM_RX       = 26;
```

#### `lib/TFT_eSPI/User_Setup.h` — Display driver and SPI pins

The active driver and pin assignments are near the top. Only change these if your display wiring differs:

```cpp
#define ILI9341_DRIVER    // Keep this; do not uncomment other drivers

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
// TFT_RST is not defined — tie the display RST pin to 3.3V or the ESP32 EN pin
```

#### `platformio.ini` — Build configuration

```ini
[env:esp-wrover-kit]
platform  = espressif32@6.2
board     = esp-wrover-kit     ; Change to your exact board if needed
framework = arduino
monitor_speed  = 115200
upload_speed   = 921600
board_build.partitions = huge_app.csv

build_flags =
    -I$PROJECT_DIR/boards/ttgo-t-call  ; See Known Build Issues below
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue      ; Required for reliable PSRAM on older silicon
```

### Known Build Issues and Fixes

#### Issue 1: Missing `boards/ttgo-t-call` directory

The `build_flags` include `-I$PROJECT_DIR/boards/ttgo-t-call`. If that directory doesn't exist the build fails with an include path error.

**Fix:** Create the directory (it can be empty if no board-specific headers are needed):
```bash
mkdir -p boards/ttgo-t-call
```
Or remove the `-I$PROJECT_DIR/boards/ttgo-t-call` line from `platformio.ini` if you don't need a board-specific header.

#### Issue 2: Missing `huge_app.csv` partition table

`board_build.partitions = huge_app.csv` tells PlatformIO to use a large-app partition layout (no OTA partitions, maximum app size). This file is usually provided by the ESP32 Arduino core.

**Fix:** If the build complains about a missing `huge_app.csv`:
```bash
# Copy from the PlatformIO ESP32 framework
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/huge_app.csv .
```
Or create a minimal one at the project root:
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
spiffs,   data, spiffs,  0x310000,0xF0000,
```

#### Issue 3: Board mismatch — `esp-wrover-kit` vs TTGO T-Call

The `board = esp-wrover-kit` definition is generic. If PlatformIO doesn't recognize your exact T-Call variant, try:
```ini
board = ttgo-t1
```
or install the [TTGO board definitions](https://github.com/espressif/arduino-esp32/tree/master/variants) manually.

#### Issue 4: PSRAM cache issue on newer ESP32 silicon

The `-mfix-esp32-psram-cache-issue` flag is a compiler workaround for a silicon errata affecting early ESP32 WROVER modules. It slightly reduces performance but prevents rare random crashes when using PSRAM. Keep it unless you know your silicon revision doesn't need it.

### Building and Flashing

**Via VS Code PlatformIO UI:** Click the checkmark (Build) then the right-arrow (Upload) icons in the bottom toolbar.

**Via CLI:**
```bash
# Build only (check for errors)
pio run

# Build and upload to the first detected device
pio run --target upload

# Specify a port explicitly (Linux/macOS)
pio run --target upload --upload-port /dev/ttyUSB0

# Specify a port (Windows)
pio run --target upload --upload-port COM3

# Erase flash before upload (useful after changing partition table)
pio run --target erase
pio run --target upload
```

### Serial Monitoring

```bash
pio device monitor --baud 115200
```

**Expected successful boot output (`DEBUG 1`):**
```
Found chip PN53x
Firmware ver. 1.6
Initializing modem...
MODEM INITIALIZED
Connecting to: web
GPRS connected
```

**Common error indicators:**
```
Didn't find PN53x board       → NFC wiring issue
Modem initialization failed.  → SIM800 power/UART issue
FAILED to connect to GPRS     → Wrong APN or no signal
```

---

## Application Flow in Detail

### Boot Sequence

```
Power On
  │
  ├─ pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW)
  ├─ Serial.begin(115200)                    [if DEBUG=1]
  ├─ tftInit()  →  TFT_eSPI.init() + rotation(1) + startScreen()
  │              └─ startScreen: 3× animation loop (27 frames × 40 ms) + logo + 3 s delay
  │
  ├─ clearScreen()
  ├─ xTaskCreatePinnedToCore(processDisplay) → FreeRTOS task starts animation
  │
  ├─ nfc.begin()  →  Wire I2C init, PN532 firmware version check
  ├─ nfc.SAMConfig()  →  configure PN532 for passive ISO14443A mode
  │   └─ if PN532 not found → display "NFC not working!" → halt forever
  │
  ├─ ttgoGsmSetup()
  │   ├─ MODEM_POWER_ON → HIGH
  │   ├─ PWRKEY pulse sequence (100 ms HIGH, 1000 ms LOW, HIGH)
  │   ├─ SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX)
  │   └─ initializeModem() loop until modem.restart() returns true
  │       └─ if fails: wait 60 s, retry indefinitely
  │
  ├─ connectGPRS()  →  up to 3 × modem.gprsConnect(APN, ...)
  │   └─ 5 s delay between retries
  │
  ├─ vTaskDelete(display_handle)     → remove animation task
  │
  ├─ [success] homePage() + gsmDisplay(true) + welcomePage(false)
  └─ [failure] gsmDisplay(false) + "Network Unavailable" + delay(40 s) + ESP.restart()
```

### State Machine

The `loop()` function dispatches to one of four states stored in `uint8_t processState`:

```
                   ┌──────────────────────────────────────┐
                   │                                      │
                   ▼                                      │
             ┌──────────┐  key=1 or 2   ┌──────────┐    │
             │  WELCOME │ ─────────────► │  AMOUNT  │    │
             └──────────┘               └──────────┘    │
                   ▲                         │ key='#'   │
                   │                         ▼            │
           10 s    │                   ┌──────────┐       │
           timer   │      card read    │   SCAN   │       │
                   │  ◄────────────────└──────────┘       │
                   │  ────────────────►                   │
             ┌──────────┐             ┌──────────┐        │
             │  NOTIFY  │ ◄────────── │   SCAN   │        │
             └──────────┘  transition └──────────┘        │
                   │                                      │
                   └──────────────────────────────────────┘
```

#### WELCOME State

**File:** `src/welcome.cpp`

Polls `kpd.getKey()` each loop iteration. Accepted keys:

| Key | Action |
|-----|--------|
| `1` | Sets `rechargeRequest = true`, transitions to AMOUNT |
| `2` | Sets `rechargeRequest = false`, transitions to AMOUNT |
| Any other | Ignored |

Screen shows:
```
Please enter:
 1. Recharge
 2. Payment
```

#### AMOUNT State

**File:** `src/amount.cpp`

Builds an integer (`sum`) from keypad presses:

| Key | Action |
|-----|--------|
| `0`–`9` | `sum = sum × 10 + digit` (capped at `AMOUNT_LIMIT / 10`) |
| `*` (ASCII 42, digit = -6) | `sum = sum / 10` (backspace) |
| `#` (ASCII 35, digit = -13) | Confirms if `sum > 1`; sets global `amount = sum`, clears `sum`, transitions to SCAN |

Screen shows:
```
Enter Recharge amount:   (or "Enter Payment amount:")
<live digit display>
```

> **Why `digit > 1` for confirm?** The check `sum > 1` prevents accidentally submitting a zero or single-digit amount.

#### SCAN State

**File:** `src/scan.cpp`

Calls `startListeningToNFC()` once on entry (resets IRQ state, calls `nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A)`).

Each loop iteration:

1. Reads `irqCurr = digitalRead(PN532_IRQ)`.
2. Waits for a **HIGH→LOW transition** on the IRQ line (card present).
3. Calls `handleCardDetected()` → reads UID bytes, builds raw string (7 bytes, reversed index order, decimal integer of each byte concatenated).
4. Calls `getHash(rawString)` → SHA-256 hex digest (64 chars).
5. Calls `merchantLogin()` → HTTPS POST, extracts `accessToken` JWT.
6. Checks `httpCode`:
   - **200** → calls `transactionRequest(accessToken, tagHash)` → transitions to NOTIFY.
   - **404** → displays "Merchant Not Found!", plays ERROR buzzer.
   - **400** → displays "Invalid Credentials!", plays ERROR buzzer.
   - **-1** → displays "No Internet", plays ERROR buzzer, returns to welcome.
   - **other** → displays "Merchant Login Failed", plays ERROR buzzer.

Screen shows:
```
Recharge Amount: 500   (or "Pay Amount: 500")
Please Scan Your Card !
```

After card detected:
```
Card Scan Successful !
P R O C E S S I N G
```

#### NOTIFY State

**File:** `src/notify.cpp`

Checks `httpCode` set by `transactionRequest()`:

| `httpCode` | Display | Buzzer |
|------------|---------|--------|
| **200** | `Dear <NAME>, your RECHARGE/PAYMENT for amount Rs.<N> is Successful!` | SUCCESS (double-beep) |
| **404** | `User Unknown or` / `Insufficient Balance` | ERROR (long beep) |
| **any other non-zero** | `Error in Transaction` | ERROR (long beep) |

`notifyTimer()` is called every loop iteration. It checks `millis() - turnOnTime > NOTIFY_TIME` (10 s). When elapsed:
- Redraws welcome page
- Updates GSM icon (checks `modem.isGprsConnected()`)
- Resets `processState = WELCOME`, `buzzMode = 0`, `amount = 0`

---

## NFC Card Processing

### Card Detection

The PN532 is configured in **passive polling mode** for **ISO 14443A** (the standard used by MIFARE Classic, MIFARE Ultralight, most transit and payment cards). Detection is interrupt-driven:

- `startListeningToNFC()` arms the PN532 to detect the next card.
- The firmware watches for a **falling edge** on GPIO 13 (PN532_IRQ) rather than polling over I2C. This is more responsive and avoids I2C traffic when no card is present.
- Once the IRQ goes LOW, `nfc.readDetectedPassiveTargetID()` retrieves the UID (4 or 7 bytes depending on card type).
- A 500 ms `DELAY_BETWEEN_CARDS` debounce is defined but the IRQ edge logic itself prevents double-reads.

### UID to Hash Conversion

The UID-to-`tagData` pipeline in `scan.cpp` and `utils.cpp`:

```
UID bytes:  [uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]]

Step 1 — Build raw string (handleCardDetected):
  for i = 6 downto 0:
      unhashTag += String( int(uid[i]) )  // e.g. "0017328514550"

Step 2 — SHA-256 hash (getHash in utils.cpp):
  payload  = "0017328514550"
  shaResult = mbedtls SHA-256(payload)    // 32-byte binary
  tagData  = hex(shaResult)               // 64-char lowercase hex string
  e.g. "a3f2c1d8e5b7..."

Step 3 — Send to backend:
  POST /api/v1/transaction/add
  { "tagData": "a3f2c1d8e5b7...", ... }
```

**PSRAM involvement:** Both the input string buffer and the 32-byte SHA output are allocated with `ps_malloc()` so they don't fragment the internal heap, then freed immediately after use.

---

## Buzzer Behavior

The buzzer is driven with non-blocking timing in `src/buzz.cpp`. All patterns start from `turnOnTime = millis()` when `playBuzz()` is called.

| Mode | Pattern | Duration |
|------|---------|----------|
| `SINGLE` | One short beep | 80 ms ON |
| `SUCCESS` | Two-tone beep | ON(0–200 ms) → OFF(200–400 ms) → ON(400–600 ms) → OFF |
| `ERROR` | One long beep | 500 ms ON |

`checkBuzzerStatus()` is called every loop iteration. It reads the current `buzzMode` and `millis()` since `turnOnTime` to decide when to turn the buzzer pin LOW.

---

## Display System

### Screen Pages

All screen functions live in `lib/DisplayFunction/displayFunction.cpp`.

| Function | When Called | Content |
|----------|-------------|---------|
| `startScreen()` | Boot | 3× loading animation + Sayapatri logo + 3 s hold |
| `showProcessing()` | Boot (FreeRTOS task) | Repeated loading animation (20×) during modem init |
| `homePage()` | After GPRS connect / returning to welcome | White background + Sayapatri logo + battery icon + charging icon |
| `welcomePage(bool)` | Entering WELCOME state | "Please enter: 1. Recharge / 2. Payment" |
| `amountPage(bool)` | Entering AMOUNT state | "Enter Recharge/Payment amount:" |
| `scanPage(bool, int)` | Entering SCAN state | "Recharge/Pay Amount: N" + "Please Scan Your Card!" |
| `lcdDisplay(msg1, msg2)` | Various | Two-line centered text in the lower panel |
| `gsmDisplay(bool)` | After GPRS state change | Cellular icon ON/OFF in top-right corner |

The TFT is **320×240 px in landscape mode** (rotation 1). The top status bar (~30 px) holds icons; the center (~70–130 px) holds the logo or animation; the lower panel (130–240 px) holds page-specific text.

### Embedded Image Assets

All images are stored as **C byte arrays in flash** (`include/images/*.h`). They are decoded at runtime using **PNGdec** line-by-line into a temporary line buffer and pushed to the TFT with `tft.pushImage()`. This avoids needing an SD card.

| Asset | Constant | Size hint |
|-------|----------|-----------|
| `logo.h` | `logo[]` | Sayapatri brand logo |
| `battery.h` | `battery[]` | Battery icon |
| `cellular.h` | `cellular[]` | Cellular signal icon |
| `charging.h` | `charge[]` | Charging indicator |
| `bluetooth.h` | `bluetooth[]` | Bluetooth icon (not currently shown) |
| `diamond.h` | `diamond[]` | Small diamond icons for pair status |
| `startAnimation.h` | `start[][]` | 27 frames × raw pixel data for the loading spinner |

### Status Icons

The top-right corner of the home/welcome/scan pages displays status icons at fixed coordinates:

| Icon | X, Y | Meaning |
|------|------|---------|
| Battery | 270, 0 | Battery level (static, not dynamic) |
| Charging | 300, 2 | Charging indicator |
| Cellular | 240, 0 | GPRS connected (shown) / disconnected (cleared) |
| Bluetooth | 215, 2 | Reserved |
| Pair diamonds | 219, 15 | Reserved for future pairing feature |

---

## API Reference

The firmware communicates with `https://rfid.test.com:443`. All requests use `Content-Type: application/json`. Responses are parsed with ArduinoJson (PSRAM-backed).

HTTP timeout is set to **5 seconds** per request (`http.setTimeout(5000)`).

---

### Authentication — Login

`POST /api/v1/auth/login`

Authenticates the merchant and returns a short-lived JWT `accessToken`. Called once per transaction (no token caching).

**Request body:**
```json
{
  "email": "merchant@example.com",
  "password": "SecurePassword1!",
  "role": "merchant"
}
```

**Successful response (HTTP 200):**
```json
{
  "accessToken": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Error responses handled by firmware:**

| HTTP Code | `httpCode` value | Firmware action |
|-----------|-----------------|-----------------|
| 200 | 200 | Extract `accessToken`, proceed to transaction |
| 400 | 400 | Display "Invalid Credentials!" |
| 404 | 404 | Display "Merchant Not Found!" |
| Network error | -1 | Display "No Internet", return to welcome |

---

### Transaction Submission

`POST /api/v1/transaction/add`

Submits the transaction. Requires a Bearer token from the login step.

**Request headers:**
```
Authorization: Bearer <accessToken>
Content-Type: application/json
```

**Request body:**
```json
{
  "tagData":        "a3f2c1d8e5b7...64hexchars...",
  "amount":         500,
  "rechargeRequest": false,
  "channel":        "pos"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tagData` | `string` | SHA-256 hex digest of the NFC card UID (64 characters) |
| `amount` | `number` | Integer amount in rupees (1 to `AMOUNT_LIMIT`) |
| `rechargeRequest` | `boolean` | `true` = recharge, `false` = payment |
| `channel` | `string` | Always `"pos"` for this device |

**Successful response (HTTP 200):**
```json
{
  "client": {
    "name": "Ramesh Sharma"
  }
}
```

The `client.name` value is displayed in the success notification: `"Dear RAMESH SHARMA, your RECHARGE for amount Rs.500 is Successful!"`

**Error responses handled by firmware:**

| HTTP Code | Firmware action |
|-----------|----------------|
| 200 | Show customer name, SUCCESS buzzer |
| 404 | Display "User Unknown or Insufficient Balance", ERROR buzzer |
| other | Display "Error in Transaction", ERROR buzzer |

---

## Test Mode

To test the complete transaction flow without a physical NFC card:

**Enable:**
```cpp
// include/config.h
#define TEST 1
```

**What happens in test mode (`scan.cpp`):**
```cpp
debugln("Running Test Case");
lcdDisplay("Running Test Case!", "");
const String accessToken = merchantLogin();   // real HTTPS call
if (accessToken != "") {
    const String tag = "ff3e3a07fee14c0562c6dba74ae7a483a8ea5a7d80ffadba5b5116971d0af47d";
    userName = transactionRequest(accessToken, tag);  // real HTTPS call
}
return 1;   // immediately transition to NOTIFY
```

- The NFC IRQ wait is **skipped entirely**.
- A **hardcoded SHA-256 hash** is used as the card tag. This hash corresponds to a specific test card registered on the backend.
- Both `merchantLogin()` and `transactionRequest()` make **real network calls**, so GPRS must still be connected.
- Useful for: testing GPRS connectivity, verifying merchant credentials, confirming backend is reachable, checking the full display+buzzer flow.

**Disable before production:**
```cpp
#define TEST 0
```

---

## Debug Mode

```cpp
// include/config.h
#define DEBUG 1   // enables
#define DEBUG 0   // disables
```

When `DEBUG = 1`, the `debug(x)` and `debugln(x)` macros expand to `Serial.print(x)` / `Serial.println(x)` at **115 200 baud**. When `DEBUG = 0`, these macros expand to nothing — zero code or flash overhead.

**Notable debug output points:**

| Location | Output |
|----------|--------|
| `main.cpp` | Modem init status, GPRS connect attempts, APN used |
| `scan.cpp` | UID bytes (hex), raw unhashed tag string, "Got NFC IRQ" |
| `httpsReq.cpp` | Full POST request body, full response body, HTTP status code |
| `buzz.cpp` | "turning off buzzer" on pattern end |
| `utils.cpp` | "Called Notify Timer" on auto-return |
| `displayFunction.cpp` | "Welcome page" / "Amount page" state labels |

---

## Security Notes

These are important for any real-world deployment:

### 1. Hardcoded merchant credentials

`MERCHANT_EMAIL` and `MERCHANT_PASSWORD` are string literals compiled directly into the firmware binary. Anyone with access to the `.bin` file can extract them with a hex editor or `strings` command.

**Recommended fix:** Store credentials in **NVS (Non-Volatile Storage)**. Provision them once via a serial setup command or BLE configuration at first boot:
```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("merchant", false);
prefs.putString("email", inputEmail);
prefs.putString("pass", inputPassword);
// Later:
String email = prefs.getString("email", "");
```

### 2. No JWT caching / expiry handling

A fresh `merchantLogin()` HTTPS call is made for every transaction. This adds ~1–2 s of latency per transaction and doubles network usage. If the backend issues short-lived tokens (e.g. 15 minutes), implement caching with an expiry timestamp.

### 3. No TLS certificate pinning

`SSLClient` performs standard chain-of-trust validation using the bundled CA store. A MITM attacker with a trusted CA could intercept traffic. For higher security, pin the server's certificate:
```cpp
// In httpsReq.cpp, before the first HTTP call:
secure_presentation_layer.setCACert(root_ca_pem);   // pin the backend's CA cert
```

### 4. Card UID determinism

The SHA-256 hash of a card's UID is always the same. If an attacker learns the tag hash (e.g. from a leaked database or network traffic), they could potentially craft a software-emulated NFC tag that produces the same UID. The backend should implement transaction-level controls (rate limiting, balance checks, anomaly detection) as the primary defence.

### 5. APN credentials in flash

`GPRS_APN`, `GPRS_USERNAME`, `GPRS_PASSWORD` in `config.h` are also compiled in. For multi-merchant deployments consider NVS storage.

### 6. No firmware update mechanism

There is no OTA update capability in this firmware. Updates require physical USB access. If deploying at scale, consider adding [ESP32 OTA via HTTPS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html).

---

## Troubleshooting

| Symptom | Root Cause | Fix |
|---------|-----------|-----|
| Screen stays white / blank | TFT SPI pins wrong or TFT_eSPI driver mismatch | Edit `lib/TFT_eSPI/User_Setup.h`; verify `ILI9341_DRIVER` and pin numbers match your wiring |
| `Didn't find PN53x board` (halts) | PN532 I2C wiring issue, wrong mode, or I2C address conflict | Check SDA/SCL connections (GPIO 21/22 default). Verify PN532 breakout is in I2C mode (solder jumpers). Try `Wire.begin(21, 22)` explicitly. |
| `Modem initialization failed` loops every 60 s | SIM800 not receiving power or PWRKEY sequence timing off | Check `MODEM_POWER_ON` (GPIO 23) and `MODEM_PWRKEY` (GPIO 4). Verify LiPo/supply can deliver 2 A peak. Try `Serial.print(modem.getSimStatus())` after `SerialAT.begin(...)`. |
| `FAILED to connect to GPRS` (3 retries then gives up) | Wrong APN, no SIM, no 2G signal | Update `GPRS_APN` in `config.h`. Confirm SIM is inserted correctly. Check for 2G coverage (many urban areas have decommissioned 2G). Try the APN settings from your carrier's documentation. |
| `Network Unavailable` screen then auto-restart after 40 s | GPRS connect failed completely | See above. If this loops indefinitely the modem is likely unresponsive or has no signal. |
| NFC card not detected | IRQ not triggering or wrong PN532 mode | Verify GPIO 13 (PN532_IRQ) is not floating. Add a 10 kΩ pull-up if needed. Check PN532 is in ISO14443A passive mode via `nfc.SAMConfig()` response. |
| Build fails: `boards/ttgo-t-call: No such file or directory` | Missing include directory | `mkdir -p boards/ttgo-t-call` or remove the `-I` line from `platformio.ini` |
| Build fails: `huge_app.csv not found` | Missing partition table file | Copy from PlatformIO ESP32 framework (see Known Build Issues section) |
| HTTP -1 (no response) | GPRS dropped mid-transaction | The `modem.isGprsConnected()` guard in `sendHttpPostRequest()` catches this. Check signal strength. |
| HTTP 400 on login | Wrong merchant password | Update `MERCHANT_PASSWORD` in `main.h` |
| HTTP 404 on login | Merchant email not registered | Update `MERCHANT_EMAIL` in `main.h` or register the merchant on the backend |
| HTTP 404 on transaction | Card UID not registered OR insufficient balance | Register the NFC card on the backend portal, or top up the account balance |
| PSRAM crash / `guru meditation` | `BOARD_HAS_PSRAM` flag missing or non-WROVER module | Ensure `build_flags` includes `-DBOARD_HAS_PSRAM`. Use an ESP32 WROVER (not WROOM) module. |
| Random resets under load | Insufficient power supply current | SIM800 can spike to 2 A. Use a LiPo battery + LDO, not USB power alone for production. |
| Keypad keys not registering | Row/column GPIO mismatch | Verify `rowPins` and `colPins` in `main.h` match your physical keypad connector order |
| Colors look wrong on display | `setSwapBytes` state mismatch | `tft.setSwapBytes(true)` for raw pixel arrays; `false` for PNG decoding. Each display function sets this explicitly. |

---

## Extending the Firmware

### Add OTA firmware updates

```cpp
#include <ArduinoOTA.h>  // or use the ESP32 HTTPS OTA library
// In setup(), after GPRS connect:
ArduinoOTA.begin();
// In loop():
ArduinoOTA.handle();
```

### Move credentials to NVS

Replace the hardcoded strings in `main.h` with NVS reads:
```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("creds", true);
MERCHANT_EMAIL    = prefs.getString("email", "");
MERCHANT_PASSWORD = prefs.getString("pass",  "");
prefs.end();
```
Add a provisioning mode (e.g. hold `*` on boot for 5 s) that accepts credentials over Serial and writes them to NVS.

### Add receipt printing

If a thermal printer is available (e.g. Adafruit Mini Thermal Printer over UART):
```cpp
// After a successful transaction:
printer.println("Sayapatri POS");
printer.println("Amount: Rs." + String(amount));
printer.println("Customer: " + userName);
printer.feed(3);
```

### Add dynamic GPRS signal strength display

```cpp
int csq = modem.getSignalQuality();  // 0–31 scale, 99=unknown
// Map csq to signal bars and update the cellular icon area
```

### Add offline queue

Store failed transactions in SPIFFS/LittleFS as JSON lines, and retry when GPRS reconnects:
```cpp
#include <SPIFFS.h>
// On transaction failure: append JSON to /queue.jsonl
// On GPRS reconnect: read and replay each line
```
