# hms-cpap-arduino

ESP8285 firmware for the FYSETC SD-WiFi v2.1 board. Serves CPAP EDF files over HTTP or pushes them to a cloud API. Arduino port of [hms-cpap-fysetc](https://github.com/hms-homelab/hms-cpap-fysetc) (ESP-IDF).

## Hardware

- **Board:** FYSETC SD-WiFi v2.1
- **MCU:** ESP8285H16 (ESP8266 core, 2 MB flash, 80 KB RAM)
- **SD card:** SPI mode, FAT16/FAT32, up to 32 GB
- **USB:** CH340E onboard (toggle switch: UART or card reader)
- **Form factor:** SD card — plugs directly into the CPAP SD slot

## Modes

| Mode | Description |
|------|-------------|
| **File Server** | ezShare-compatible HTTP server on port 80. Browse and download EDF files from any device on the network. |
| **Cloud Push** | Scans DATALOG/, syncs with cloud API, uploads new/changed EDF files in 4 KB chunks. |

Mode is selected via the WiFiManager captive portal on first boot.

## Quick Start

### Build

```bash
# Install PlatformIO
pip install platformio

# Build
cd projects/hms-cpap-arduino
platformio run
```

### Flash

1. Set the board toggle switch to **USB2UART**
2. Hold the **FLASH** button, plug in USB, release after 2 seconds
3. Flash:

```bash
platformio run --target upload
# or manually:
esptool.py --port /dev/ttyUSB1 --baud 460800 --chip esp8266 \
    write_flash 0x0 .pio/build/sleeplink/firmware.bin
```

### First-Time Setup

1. After flashing, the board creates a `SleepLink-XXXX` WiFi network
2. Connect your phone to it
3. A captive portal opens — select your WiFi network, enter password
4. Choose operating mode (File Server or Cloud Push)
5. Save — device reboots and connects to your network

### Usage (File Server mode)

```bash
# Browse DATALOG
curl http://<device-ip>/dir?dir=A:/DATALOG

# Download a file
curl -o session.edf http://<device-ip>/download?file=/DATALOG/20260308/20260308_222649_BRP.edf

# Device status
curl http://<device-ip>/api/status
```

## API Endpoints (File Server mode)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Landing page with links |
| `/dir?dir=A:/path` | GET | ezShare-compatible directory listing |
| `/download?file=/path` | GET | Raw file download (supports Range header) |
| `/api/status` | GET | JSON status (heap, uptime, WiFi, version) |

## API Endpoints (Cloud Push mode)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/data/sync` | POST | Manifest sync — get server offsets |
| `/api/v1/data/edf` | POST | Upload EDF file chunk (4 KB) |
| `/api/v1/data/str` | POST | Upload STR.edf chunk |
| `/api/v1/device/heartbeat` | POST | Periodic check-in |

## Pin Configuration

| Pin | Function |
|-----|----------|
| GPIO 4 | SD card CS |
| GPIO 12 | SPI MISO |
| GPIO 13 | SPI MOSI |
| GPIO 14 | SPI SCLK |
| GPIO 5 | CS sense (external master detection) |
| GPIO 0 | Boot/Flash button (hold 5s = reset) |
| GPIO 2 | LED (active low) |

## Reset

Hold the **FLASH** button for 5 seconds. The LED blinks rapidly, WiFi credentials and checkpoints are cleared, and the device reboots into captive portal mode.

## Build Info

- Framework: Arduino (espressif8266)
- Board target: `esp8285` (2 MB flash, DIO mode)
- RAM usage: ~48% (39 KB / 80 KB)
- Flash usage: ~47% (491 KB / 1044 KB)

## Dependencies

- [WiFiManager](https://github.com/tzapu/WiFiManager) 2.0.17
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.x
- [ESP8266SdFat](https://github.com/earlephilhower/ESP8266SdFat) 2.2.2
- [NTPClient](https://github.com/arduino-libraries/NTPClient)
- [Time](https://github.com/PaulStoffregen/Time)

## Related Projects

- [hms-cpap-fysetc](https://github.com/hms-homelab/hms-cpap-fysetc) — ESP-IDF version (ESP32, FYSETC SD WiFi Pro)
- [hms-cpap](https://github.com/hms-homelab/hms-cpap) — CPAP data collection service (C++)
- [hms-sleeplink-parser](https://github.com/hms-homelab/hms-sleeplink-parser) — EDF parser library
