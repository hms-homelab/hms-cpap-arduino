# Changelog

## 2026.1.1 (2026-05-22)

Rebrand and cleanup release. No behavior change for File Server users.

### Changed

- Rebrand SleepLink → CpapDash across source, captive-portal AP name (`CpapDash-XXXX`), mDNS hostname (`cpapdash`), and device serial prefix (`CD-XXXXXXXX`)
- Default API URL updated from defunct `https://api.sleeplinkusa.com` to `https://api.cpapdash.com`
- Embedded firmware version aligned with repo `YYYY.ver.patch` convention (was `1.0.0`)
- Captive portal: removed the File Server / Cloud Push mode picker. Cloud Push is unviable on ESP8285 (BearSSL + WiFi stack exhausts the ~40 KB free heap during chunked HTTPS uploads — see [docs/ESP8285_CONSTRAINTS.md](docs/ESP8285_CONSTRAINTS.md)). File Server mode is now forced on every boot.

### Docs

- README rewritten to match `hms-fysetc` pattern (EXPERIMENTAL banner, badges, ASCII diagram, License section)
- Added MIT `LICENSE`
- Renamed `docs/SLEEPLINK_HARDWARE_OPTIONS.md` → `docs/HARDWARE_OPTIONS.md`

## 2026.1.0 (2026-04-06)

Initial release — ESP8285 dual-mode CPAP data bridge.

### Features

- **File Server mode (default):** ezShare-compatible HTTP server on port 80
  - `GET /dir?dir=A:/DATALOG` — directory listing
  - `GET /download?file=...` — file download with Range header support
  - `GET /api/status` — JSON device status
  - Chunked streaming at ~137 KB/s for multi-MB EDF files
- **Cloud Push mode:** autonomous upload to CpapDash API
  - Sync handshake (`POST /api/v1/data/sync`) — skip already-uploaded files
  - Chunked EDF upload (4 KB chunks) with retry + exponential backoff
  - STR.edf upload
  - Heartbeat with remote reset support
  - EEPROM-based file offset checkpoints
- **WiFiManager captive portal:** dark theme, WiFi scan dropdown
- **NTP time sync**
- **Boot button reset:** hold 5s to clear WiFi + config
- **Device serial:** auto-generated from chip ID (`CD-XXXXXXXX`)

### Hardware

- Board: FYSETC SD-WiFi v2.1 (ESP8285H16, 2 MB flash, 80 KB RAM)
- SD card: SPI mode via SdFat 2.2.2 (FAT16/FAT32, up to 32 GB)
- SPI pins: CS=4, MISO=12, MOSI=13, SCLK=14
- SHARED_SPI mode for reliable mount/unmount cycles

### Build

- PlatformIO + Arduino framework (espressif8266)
- RAM: 48% (39 KB), Flash: 47% (491 KB)
- Libraries: WiFiManager 2.0.17, ArduinoJson 6, ESP8266SdFat 2.2.2, NTPClient
