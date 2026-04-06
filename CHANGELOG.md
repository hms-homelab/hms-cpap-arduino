# Changelog

## 2026.1.0 (2026-04-06)

Initial release — ESP8285 dual-mode CPAP data bridge.

### Features

- **File Server mode (default):** ezShare-compatible HTTP server on port 80
  - `GET /dir?dir=A:/DATALOG` — directory listing
  - `GET /download?file=...` — file download with Range header support
  - `GET /api/status` — JSON device status
  - Chunked streaming at ~137 KB/s for multi-MB EDF files
- **Cloud Push mode:** autonomous upload to SleepLink API
  - Sync handshake (`POST /api/v1/data/sync`) — skip already-uploaded files
  - Chunked EDF upload (4 KB chunks) with retry + exponential backoff
  - STR.edf upload
  - Heartbeat with remote reset support
  - EEPROM-based file offset checkpoints
- **WiFiManager captive portal:** dark theme, WiFi scan dropdown
- **NTP time sync**
- **Boot button reset:** hold 5s to clear WiFi + config
- **Device serial:** auto-generated from chip ID (`SL-XXXXXXXX`)

### Hardware

- Board: FYSETC SD-WiFi v2.1 (ESP8285H16, 2 MB flash, 80 KB RAM)
- SD card: SPI mode via SdFat 2.2.2 (FAT16/FAT32, up to 32 GB)
- SPI pins: CS=4, MISO=12, MOSI=13, SCLK=14
- SHARED_SPI mode for reliable mount/unmount cycles

### Build

- PlatformIO + Arduino framework (espressif8266)
- RAM: 48% (39 KB), Flash: 47% (491 KB)
- Libraries: WiFiManager 2.0.17, ArduinoJson 6, ESP8266SdFat 2.2.2, NTPClient
