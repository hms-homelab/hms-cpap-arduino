# hms-cpap-arduino

> **EXPERIMENTAL** -- This board is a ESP8285 is right at the edge of what's possible. File Server mode is reliable; cloud push is not. See [docs/ESP8285_CONSTRAINTS.md](docs/ESP8285_CONSTRAINTS.md).

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Arduino](https://img.shields.io/badge/Platform-Arduino-00979D.svg?logo=arduino)](https://www.arduino.cc/)
[![Board: ESP8285](https://img.shields.io/badge/Board-ESP8285-blue.svg?logo=espressif)](https://www.espressif.com/en/products/socs/esp8266)
[![Status: Experimental](https://img.shields.io/badge/Status-Experimental-orange.svg)]()

Turn a [$9 FYSETC SD-WiFi v2.1](https://www.fysetc.com/products/fysetc-sd-wifi-card) into a WiFi bridge for your ResMed CPAP machine's SD card. Serves CPAP sleep data over HTTP on your home network — no cloud, no subscription, no app.

Arduino port of [hms-fysetc](https://github.com/hms-homelab/hms-fysetc) (ESP-IDF). The cheaper SD-WiFi v2.1 fits the same socket but uses an ESP8285 with only 80 KB RAM, so the Arduino port is leaner: file serving works fine, HTTPS cloud push does not (BearSSL + WiFi stack exhausts the heap during chunked uploads).

## How It Works

```
CPAP SD slot
    |
    v
┌──────────────────────┐
│  FYSETC SD-WiFi v2.1 │  ← Plugs into CPAP's SD slot
│  (ESP8285 + SD card) │     Joins your home WiFi
│                      │     Serves files over HTTP
└──────────────────────┘
    |
    v  (HTTP over WiFi)
┌──────────────────────┐
│  hms-cpap            │  ← Polls every 65 seconds
│  (Pi or any server)  │     Parses EDF files
│                      │     Publishes to Home Assistant
└──────────────────────┘
```

The firmware is an **ezShare-compatible HTTP file server**. It emulates the [ez Share WiFi SD](http://www.ezshare.com/) card's HTTP API, so [hms-cpap](https://github.com/hms-homelab/hms-cpap) works with zero code changes — just point the URL at the FYSETC's IP.

**SD bus safety:** The SD card is mounted read-only and only while serving a request. Between requests, the bus is released back to the CPAP. The CPAP and ESP8285 share the bus via software arbitration (CS-sense interrupt + tri-state pins). See [docs/BUS_SHARING.md](docs/BUS_SHARING.md).

## HTTP API (ez Share compatible)

| Endpoint | Description |
|----------|-------------|
| `GET /` | Landing page with links |
| `GET /dir?dir=A:DATALOG` | HTML directory listing of date folders |
| `GET /dir?dir=A:DATALOG%5CYYYYMMDD` | HTML file listing for a date folder |
| `GET /download?file=DATALOG%5CYYYYMMDD%5Cfilename` | Raw file download (supports `Range` header) |
| `GET /api/status` | JSON device status (heap, uptime, WiFi, version) |

Path separators use `%5C` (backslash) in query parameters, matching real ez Share hardware.

## Hardware

| | |
|---|---|
| **Board** | [FYSETC SD-WiFi v2.1](https://www.fysetc.com/products/fysetc-sd-wifi-card) (~$9) |
| **MCU** | ESP8285H16 (ESP8266 core, 2 MB flash, 80 KB RAM) |
| **SD card** | SPI mode, FAT16/FAT32, up to 32 GB |
| **USB** | CH340E onboard (toggle switch: UART or card reader) |
| **Form factor** | SD card — plugs directly into the CPAP SD slot |

See [docs/HARDWARE.md](docs/HARDWARE.md) for pinout and detailed board notes, and [docs/HARDWARE_OPTIONS.md](docs/HARDWARE_OPTIONS.md) for a comparison with other CPAP-bridge hardware.

## Quick Start

### 1. Install PlatformIO

```bash
pip install platformio
```

### 2. Build

```bash
git clone https://github.com/hms-homelab/hms-cpap-arduino.git
cd hms-cpap-arduino
platformio run
```

### 3. Flash

1. Set the board toggle switch to **USB2UART**
2. Hold the **FLASH** button, plug in USB, release after 2 seconds
3. Flash:

```bash
platformio run --target upload
```

Or via esptool directly:

```bash
esptool.py --port /dev/ttyUSB1 --baud 460800 --chip esp8266 \
    write_flash 0x0 .pio/build/cpapdash/firmware.bin
```

### 4. First-Time Setup

1. After flashing, the board creates a `CpapDash-XXXX` WiFi network
2. Connect your phone to it
3. A captive portal opens — select your home WiFi network, enter the password
4. Save — device reboots and connects to your network

Credentials persist in EEPROM. To re-enter setup, hold the **FLASH** button for 5 seconds (LED blinks rapidly, WiFi credentials and checkpoints are cleared, device reboots into captive portal mode).

### 5. Use It

```bash
# Browse DATALOG
curl http://<device-ip>/dir?dir=A:DATALOG

# Download a file
curl -o session.edf "http://<device-ip>/download?file=DATALOG%5C20260308%5C20260308_222649_BRP.edf"

# Device status
curl http://<device-ip>/api/status
```

Or just point [hms-cpap](https://github.com/hms-homelab/hms-cpap) at `http://<device-ip>` and it will collect data automatically.

## Build Info

- Framework: Arduino (espressif8266)
- Board target: `esp8285` (2 MB flash, DIO mode)
- RAM usage: ~48% (39 KB / 80 KB)
- Flash usage: ~47% (491 KB / 1044 KB)
- No OTA — 2 MB flash cannot fit two firmware images (see [docs/ESP8285_CONSTRAINTS.md](docs/ESP8285_CONSTRAINTS.md))

## Documentation

- [Hardware reference](docs/HARDWARE.md) — pinout, levels, current draw
- [Hardware options](docs/HARDWARE_OPTIONS.md) — when to pick this board vs alternatives
- [ESP8285 constraints](docs/ESP8285_CONSTRAINTS.md) — RAM/flash budget, why HTTPS doesn't work
- [Bus sharing](docs/BUS_SHARING.md) — how the ESP and CPAP coexist on the SD bus
- [CPAP bus investigation](docs/CPAP_BUS_INVESTIGATION.md) — debugging notes when CPAP errors out
- [Firmware porting](docs/FIRMWARE_PORTING.md) — ESP-IDF → Arduino translation notes

## Dependencies

- [WiFiManager](https://github.com/tzapu/WiFiManager) 2.0.17
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.x
- [ESP8266SdFat](https://github.com/earlephilhower/ESP8266SdFat) 2.2.2
- [NTPClient](https://github.com/arduino-libraries/NTPClient)
- [Time](https://github.com/PaulStoffregen/Time)

## Related Projects

- [hms-fysetc](https://github.com/hms-homelab/hms-fysetc) — Generic file-server firmware for the FYSETC SD WiFi Pro (ESP32, ESP-IDF) — better board, better firmware, recommended for production
- [hms-cpap](https://github.com/hms-homelab/hms-cpap) — CPAP data collection service (C++ / MQTT / Home Assistant)
- [hms-cpapdash-parser](https://github.com/hms-homelab/hms-cpapdash-parser) — EDF parser library

## License

MIT — see [LICENSE](LICENSE).
