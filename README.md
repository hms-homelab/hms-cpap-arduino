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

## Is this the right firmware for my board?

FYSETC sells two SD-WiFi cards that look similar but are very different. This firmware is for the **cheaper one with a USB-C port and a toggle switch**:

| Product | MCU | Toggle switch | Use this firmware? |
|---------|-----|---------------|--------------------|
| **FYSETC SD-WiFi v2.1** (~$9, card-reader/upload toggle, USB-C) | ESP8285 | yes | ✅ Yes — this repo |
| **FYSETC SD-WiFi Pro** (~$28, dedicated dongle board with ESP32) | ESP32 | no | ❌ No — use [hms-fysetc](https://github.com/hms-homelab/hms-fysetc) (ESP-IDF) |
| **ezShare WiFi SD** (real ezShare card) | n/a | n/a | ❌ No — works out of the box with [hms-cpap](https://github.com/hms-homelab/hms-cpap) |

If your card has a small black toggle switch on its edge and a USB-C port, you're in the right place.

## Quick Start (pre-built firmware)

The easiest path. No build tools, no compiler — just flash the released `firmware.bin` and you're done.

### 1. Download the firmware

Grab `firmware.bin` from the latest [release](https://github.com/hms-homelab/hms-cpap-arduino/releases).

### 2. Install the CH340 USB driver (macOS / Windows only)

The board's USB-to-serial chip is a CH340E. macOS Sonoma+ and recent Windows have the driver built-in. If your computer doesn't see the device after plugging it in, install from:

- macOS: <https://www.wch.cn/downloads/CH341SER_MAC_ZIP.html>
- Windows: <https://www.wch.cn/downloads/CH341SER_EXE.html>
- Linux: works out of the box (`/dev/ttyUSB0` or `/dev/ttyACM0`)

### 3. Put the board in flash mode

1. **Insert your micro SD card** into the FYSETC board (formatted FAT32, up to 32 GB)
2. Move the small **toggle switch** on the board edge to **USB2UART** (the position closer to the USB-C port)
3. **Hold the FLASH button**, plug in USB-C, release the button after 2 seconds — the LED stays off, indicating bootloader mode

### 4. Flash

The simplest option is the **browser-based** [ESP Web Flasher](https://espressif.github.io/esptool-js/) (works on Chrome/Edge):

1. Open <https://espressif.github.io/esptool-js/>
2. Click **Connect** and pick the CH340 serial port
3. Click **Add File**, select the `firmware.bin` you downloaded, set address to `0x0`
4. Click **Program**

Or use [`esptool`](https://github.com/espressif/esptool) from the command line:

```bash
pip install esptool
esptool.py --port /dev/cu.usbserial-XXXX --baud 460800 --chip esp8266 \
    write_flash 0x0 firmware.bin
```

(Port name: macOS `/dev/cu.usbserial-*`, Linux `/dev/ttyUSB0`, Windows `COM3` or similar.)

### 5. Switch back to runtime mode

Unplug USB, **move the toggle switch back to SD/Upload** (away from USB-C), and re-plug. The LED blinks briefly — the board is now running.

## Build from source (optional)

Only if you want to modify the firmware.

```bash
pip install platformio
git clone https://github.com/hms-homelab/hms-cpap-arduino.git
cd hms-cpap-arduino
platformio run                       # builds .pio/build/cpapdash/firmware.bin
platformio run --target upload       # flashes (board must be in flash mode)
```

## First-Time Setup

1. Insert the board into your CPAP's SD slot (or any USB power source for testing)
2. On your phone, connect to the `CpapDash-XXXX` WiFi network (where `XXXX` is the last 4 chars of the device's chip ID)
3. A captive portal opens automatically — select your home WiFi network, enter the password
4. Save — device reboots and connects to your home network

Credentials persist in EEPROM. To re-enter setup, hold the **FLASH** button for 5 seconds (LED blinks rapidly, WiFi credentials and checkpoints are cleared, device reboots into captive portal mode).

## Usage

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
