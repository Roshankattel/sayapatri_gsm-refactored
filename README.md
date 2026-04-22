# Sayapatri GSM — ESP32 Cellular POS Terminal Firmware

A complete Point-of-Sale (POS) terminal firmware for an **ESP32-based device** that combines a cellular modem (SIM800), NFC card reader (PN532), a matrix keypad, a TFT display, and a buzzer into a self-contained payment and recharge terminal. It communicates with a backend server over **GPRS/HTTPS** to process card-based transactions without requiring Wi-Fi.

---

## Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Pin Configuration](#pin-configuration)
- [Software Dependencies](#software-dependencies)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [Configuration](#configuration)
  - [Building and Flashing](#building-and-flashing)
  - [Serial Monitoring](#serial-monitoring)
- [Application Flow](#application-flow)
- [State Machine](#state-machine)
- [API Reference](#api-reference)
- [Test Mode](#test-mode)
- [Security Notes](#security-notes)
- [Troubleshooting](#troubleshooting)

---

## Overview

The device acts as a **merchant terminal** under the **Sayapatri** brand. A merchant uses the keypad to select a transaction type (payment or recharge) and enter an amount. The customer then taps an **NFC/RFID card** on the reader. The firmware:

1. Reads the card's UID.
2. Derives a **SHA-256 hash** from the UID bytes.
3. Authenticates with the backend (JWT login) over cellular GPRS.
4. Submits the transaction.
5. Displays the result (customer name, success, or error) on the TFT screen and plays a buzzer pattern.

The device reconnects to GPRS automatically on startup and restarts if the network is unavailable.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     ESP32 (WROVER)                      │
│                                                         │
│  ┌──────────┐   ┌──────────┐   ┌──────────────────┐   │
│  │  Keypad  │   │  PN532   │   │  ILI9341 TFT     │   │
│  │ 4×3 GPIO │   │  (I2C +  │   │  Display (SPI)   │   │
│  │  Matrix  │   │   IRQ)   │   │  240×320         │   │
│  └──────────┘   └──────────┘   └──────────────────┘   │
│                                                         │
│  ┌──────────┐   ┌─────────────────────────────────┐   │
│  │  Buzzer  │   │  SIM800 GSM/GPRS Modem (UART)   │   │
│  │  (GPIO)  │   │  TinyGSM → SSLClient → HTTPS    │   │
│  └──────────┘   └─────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                              │
                           GPRS
                              │
                   ┌──────────────────┐
                   │  rfid.go.com     │
                   │  (HTTPS :443)    │
                   │  REST JSON API   │
                   └──────────────────┘
```

---

## Hardware Requirements

| Component | Part / Model | Notes |
|-----------|-------------|-------|
| Microcontroller | **ESP32 WROVER** (with PSRAM) | TTGO T-Call V1.x form factor wiring used in code |
| Cellular Modem | **SIM800** / SIM800L / SIM800C | Integrated on TTGO T-Call or external; 2G GPRS |
| NFC Reader | **NXP PN532** (Adafruit breakout) | Wired over I2C; IRQ line required |
| Display | **ILI9341** 240×320 TFT | SPI connection |
| Input | **4×3 Matrix Keypad** | Standard 12-key layout (`1–9`, `*`, `0`, `#`) |
| Buzzer | Passive or active buzzer | Single GPIO drive |
| SIM Card | Any 2G-capable SIM with data | Ensure APN is set correctly in `config.h` |
| Power | 3.7 V LiPo or regulated 3.3 V/5 V supply | SIM800 requires up to 2 A peak |

> **Note:** The codebase targets the **TTGO T-Call** pin layout. If you use a different ESP32 board, you must update the modem and TFT pin definitions accordingly.

---

## Pin Configuration

### GSM Modem (SIM800 / TTGO T-Call)

| Signal | GPIO |
|--------|------|
| `MODEM_PWRKEY` | 4 |
| `MODEM_RST` | 5 |
| `MODEM_POWER_ON` | 23 |
| `MODEM_TX` (ESP → Modem) | 27 |
| `MODEM_RX` (Modem → ESP) | 26 |

### NFC Reader (PN532)

| Signal | GPIO |
|--------|------|
| `PN532_IRQ` | 13 |
| `PN532_RESET` | 25 |
| SDA | 21 (default ESP32 I2C) |
| SCL | 22 (default ESP32 I2C) |

### TFT Display (ILI9341 SPI)

| Signal | GPIO |
|--------|------|
| `TFT_MOSI` | 19 |
| `TFT_SCLK` | 18 |
| `TFT_CS` | 15 |
| `TFT_DC` | 2 |

> TFT pins are configured in `lib/TFT_eSPI/User_Setup.h`. Edit this file if your wiring differs.

### Keypad (4×3 Matrix)

| Line | GPIOs |
|------|-------|
| Rows (R1–R4) | 34, 36, 39, 35 |
| Columns (C1–C3) | 32, 14, 33 |

### Buzzer

| Signal | GPIO |
|--------|------|
| `BUZZER_PIN` | 12 |

---

## Software Dependencies

### PlatformIO Library Registry (`lib_deps` in `platformio.ini`)

| Library | Version | Purpose |
|---------|---------|---------|
| `adafruit/Adafruit PN532` | `^1.2.3` | NFC/RFID card reading |
| `chris--a/Keypad` | `^3.1.1` | Matrix keypad scanning |
| `bitbank2/PNGdec` | `^1.0.1` | PNG image decode for TFT |
| `bblanchon/ArduinoJson` | `^6.19.3` | JSON serialization/deserialization |
| `vshymanskyy/TinyGSM` | `^0.11.5` | AT-command abstraction for SIM800 |
| `arduino-libraries/ArduinoHttpClient` | `^0.4.0` | HTTP(S) client over TinyGSM transport |

### Vendored Libraries (`lib/` directory)

| Library | Purpose |
|---------|---------|
| `TFT_eSPI` | Full Bodmer TFT driver for ILI9341 |
| `SSLClient` | TLS layer on top of TinyGsmClient (enables HTTPS) |
| `DisplayFunction` | Project-specific UI rendering (screens, PNG assets, animation) |

### ESP-IDF / Arduino Built-ins

| Component | Usage |
|-----------|-------|
| `mbedtls` | SHA-256 hashing of NFC card UIDs |
| `FreeRTOS` | Background task for the "processing" animation |
| `PSRAM` | Large JSON document allocation via `SpiRamJsonDocument` |

---

## Project Structure

```
sayapatri_gsm-refactored/
├── platformio.ini              # PlatformIO build configuration
├── include/
│   ├── config.h                # App-wide constants: server, pins, APN, limits
│   ├── main.h                  # Global variables, pin maps, modem & NFC instances
│   ├── gsmSetup.h              # SerialAT, TinyGSM modem type, library includes
│   ├── httpReq.h               # Declarations for HTTP helper functions
│   ├── psram.h                 # SpiRamJsonDocument allocator helper
│   ├── utils.h / buzz.h        # Utility and buzzer module declarations
│   ├── welcome.h / amount.h    # UI state declarations
│   └── scan.h / notify.h       # NFC scan and notification declarations
│
├── src/
│   ├── main.cpp                # setup(), loop(), modem power-on, GPRS connect
│   ├── welcome.cpp             # Keypad: select payment (2) or recharge (1)
│   ├── amount.cpp              # Keypad: numeric entry, backspace (*), confirm (#)
│   ├── scan.cpp                # NFC IRQ detection, UID read, login + transaction
│   ├── httpsReq.cpp            # merchantLogin() and transactionRequest() via HTTPS
│   ├── notify.cpp              # Display result, buzzer pattern, auto-return timer
│   ├── buzz.cpp                # Non-blocking buzzer: SINGLE / SUCCESS / ERROR
│   └── utils.cpp               # lcdDisplay(), notifyTimer(), getHash() (SHA-256)
│
├── lib/
│   ├── DisplayFunction/
│   │   ├── displayFunction.h/.cpp   # TFT screen pages, PNG rendering, animation
│   │   └── images/                  # Embedded PNG headers (logo, battery, icons…)
│   ├── SSLClient/              # Vendored TLS wrapper for TinyGSM transport
│   └── TFT_eSPI/               # Bodmer TFT_eSPI driver + User_Setup.h (pin config)
│
└── test/                       # PlatformIO unit test placeholder
```

---

## Getting Started

### Prerequisites

1. **[PlatformIO](https://platformio.org/)** — Install via VS Code extension or the PlatformIO CLI.
   ```bash
   pip install platformio
   ```
2. **USB driver** for your ESP32 board (CP2102 or CH340, depending on the board).
3. A **SIM card** with 2G/GPRS data service active. Insert it into the SIM800 SIM tray before powering up.

### Installation

```bash
# Clone the repository
git clone https://github.com/<your-org>/sayapatri_gsm-refactored.git
cd sayapatri_gsm-refactored
```

### Configuration

Edit the following files before building:

#### `include/config.h`

```cpp
#define DEBUG 1          // Set to 0 to disable serial debug output
#define TEST  0          // Set to 1 to use dummy NFC data (no real card needed)

const char hostname[] = "";  // Backend server hostname
const int  port       = 443;                  // HTTPS port

const uint8_t PN532_IRQ   = 13;  // NFC IRQ pin
const uint8_t PN532_RESET = 25;  // NFC reset pin

const char GPRS_APN[16]      = "web";  // Replace with your SIM's APN
const char GPRS_USERNAME[16] = "";     // APN username (often empty)
const char GPRS_PASSWORD[16] = "";     // APN password (often empty)
```

#### `include/main.h`

```cpp
String MERCHANT_EMAIL    = "ajay@gmail.com";  // Replace with real merchant email
String MERCHANT_PASSWORD = "Hello123@";        // Replace with real merchant password
```

> **Security:** These credentials are compiled into the firmware. See [Security Notes](#security-notes) for best practices.

#### `lib/TFT_eSPI/User_Setup.h`

Verify that the TFT driver (`ILI9341_DRIVER`) and SPI pin definitions match your actual wiring. The defaults are:

```cpp
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
```

#### `platformio.ini` — Board Mismatch

The `board = esp-wrover-kit` entry works for most ESP32 WROVER modules. If you use a **TTGO T-Call** board, you may need to add a custom board definition or swap to `board = ttgo-t1`. The `build_flags` line `-I$PROJECT_DIR/boards/ttgo-t-call` expects a `boards/ttgo-t-call` directory in the project root — create it if your display pins live in a board-specific header, or remove the flag if you do not need it.

Ensure `huge_app.csv` is available. Either:
- Use the one from the ESP32 Arduino core (`~/.platformio/packages/framework-arduinoespressif32/tools/partitions/huge_app.csv`), or
- Create a `huge_app.csv` in the project root with an appropriate partition table.

### Building and Flashing

```bash
# Build only
pio run

# Build and upload to connected device
pio run --target upload

# Upload with explicit port
pio run --target upload --upload-port /dev/ttyUSB0
```

Or use the **PlatformIO toolbar** in VS Code (Build ✓ / Upload → icons).

### Serial Monitoring

```bash
pio device monitor --baud 115200
```

On a successful boot you should see:

```
Found chip PN53x
Firmware ver. X.Y
Initializing modem...
MODEM INITIALIZED
Connecting to: web
GPRS connected
```

---

## Application Flow

```
Power On
   │
   ▼
Buzzer Init → TFT Init → NFC (PN532) Init
   │
   ▼
GSM Modem Power Sequence (PWRKEY pulse)
   │
   ▼
TinyGSM restart → GPRS connect (up to 3 retries)
   │              If all fail → display "Network Unavailable" → restart after 40 s
   ▼
Show Home Page + GSM icon
   │
   ▼
┌─────────────────────────────────────────────┐
│              Main Loop (State Machine)       │
│                                             │
│  WELCOME ──(key 1 or 2)──► AMOUNT           │
│  AMOUNT  ──(# confirm)───► SCAN             │
│  SCAN    ──(card read)───► NOTIFY           │
│  NOTIFY  ──(10 s timer)──► WELCOME          │
└─────────────────────────────────────────────┘
```

---

## State Machine

### `WELCOME`

Displays the welcome screen. The merchant presses:
- **`1`** → Recharge flow (`rechargeRequest = true`)
- **`2`** → Payment flow (`rechargeRequest = false`)

Transitions to `AMOUNT`.

### `AMOUNT`

Merchant enters the transaction amount using the keypad:
- **`0–9`** → Append digit (max `AMOUNT_LIMIT = 1,000,000`)
- **`*`** → Backspace (delete last digit)
- **`#`** → Confirm and proceed

Transitions to `SCAN`.

### `SCAN`

The device listens for an NFC card via the **PN532 IRQ pin**:

1. On IRQ falling edge, reads the ISO 14443A card UID.
2. Builds a raw string from the UID bytes.
3. Hashes the string with **SHA-256** (`getHash()` using mbedtls).
4. Calls `merchantLogin()` → obtains a JWT `accessToken`.
5. Calls `transactionRequest(accessToken, tagHash)` → posts amount, type, and card hash.
6. Parses the response to get the customer name.

Transitions to `NOTIFY`.

### `NOTIFY`

Displays the outcome on screen and plays a buzzer pattern:

| HTTP Result | Display | Buzzer |
|-------------|---------|--------|
| 200 OK | Customer name + "Success" | SUCCESS pattern |
| 404 Not Found | "Card not registered" | ERROR pattern |
| Other error | "Transaction failed" | ERROR pattern |
| Network error | "Network error" | ERROR pattern |

After `NOTIFY_TIME` (10 000 ms), automatically returns to `WELCOME`.

---

## API Reference

The firmware communicates with **`https://rfid.tezx.com`** using two endpoints.

### `POST /api/v1/auth/login`

Authenticates the merchant and returns a short-lived JWT.

**Request body:**
```json
{
  "email": "merchant@example.com",
  "password": "password123",
  "role": "merchant"
}
```

**Response (200):**
```json
{
  "accessToken": "<JWT>"
}
```

---

### `POST /api/v1/transaction/add`

Submits a card transaction.

**Request headers:**
```
Authorization: Bearer <accessToken>
Content-Type: application/json
```

**Request body:**
```json
{
  "tagData": "<sha256-hex-of-uid>",
  "amount": 500,
  "rechargeRequest": false,
  "channel": "pos"
}
```

**Response (200):**
```json
{
  "client": {
    "name": "Customer Name"
  }
}
```

---

## Test Mode

To test the full firmware flow without a physical NFC card, set `TEST 1` in `config.h`:

```cpp
#define TEST 1
```

`scan.cpp` will skip the NFC IRQ wait and use a hardcoded dummy tag hash. The login and transaction HTTP calls still execute against the real backend, so you can verify network connectivity and API correctness without any card hardware.

Remember to set `TEST 0` before deploying to production.

---

## Security Notes

> **Important:** Review these points before deploying to a production environment.

1. **Hardcoded credentials** — `MERCHANT_EMAIL` and `MERCHANT_PASSWORD` in `main.h` are compiled directly into the firmware binary. Anyone with the `.bin` file can extract them. Consider:
   - Storing credentials in NVS (Non-Volatile Storage) and provisioning them over serial or BLE at first boot.
   - Using a device-specific secret derived from the ESP32 eFuse MAC address.

2. **No certificate pinning** — `SSLClient` verifies the server certificate chain using the bundled trust store. For extra security, pin the server's leaf or CA certificate in `lib/SSLClient/`.

3. **GPRS APN credentials** — Also hardcoded in `config.h`. For multi-tenant deployments, store these in NVS.

4. **Card hash** — The UID-derived SHA-256 hash sent as `tagData` is deterministic. If a UID is leaked, it can be replayed. The backend should implement anti-replay measures (nonces, timestamps).

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `Didn't find PN53x board` on boot | I2C wiring issue or wrong SDA/SCL pins | Check I2C connections; default ESP32 is SDA=21, SCL=22 |
| `Modem initialization failed` loop | SIM800 not powered / PWRKEY timing | Verify `MODEM_POWER_ON` (GPIO 23) and `MODEM_PWRKEY` (GPIO 4) wiring; check supply current |
| `FAILED to connect to GPRS` | Wrong APN or no SIM data | Update `GPRS_APN` in `config.h`; confirm SIM has data plan |
| `Network Unavailable` → restart | GPRS connect failed after 3 retries | See above; also ensure 2G network is available in your area |
| Build error: missing `boards/ttgo-t-call` | `-I` path in `build_flags` does not exist | Create the `boards/ttgo-t-call/` directory or remove that flag |
| Build error: missing `huge_app.csv` | Partition file not found | Copy from framework path or create your own partition CSV |
| TFT shows garbage / no display | Wrong SPI pins or TFT driver | Edit `lib/TFT_eSPI/User_Setup.h` to match your wiring |
| HTTP 401 on transaction | Expired or invalid JWT | Check merchant credentials in `main.h`; backend token lifetime |
| PSRAM allocation crash | Board without PSRAM or flag missing | Ensure `BOARD_HAS_PSRAM` is in `build_flags`; use a WROVER module |

---

## License

This project does not currently include a license file. Contact the repository owner for usage terms.
