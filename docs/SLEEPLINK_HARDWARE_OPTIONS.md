# SleepLink Hardware Options

Comparison of all hardware approaches for getting CPAP data off the SD card.

## Option 1: FYSETC SD WiFi Pro ($28)

**Board:** ESP32-PICO-D4, 4 MB flash, 2 MB PSRAM, hardware MUX
**Firmware:** hms-cpap-fysetc / hms-sleeplink-firmware (ESP-IDF)
**Form factor:** SD card (fits directly in CPAP slot)

| Aspect | Details |
|--------|---------|
| Bus sharing | Hardware MUX (GPIO26) — reliable concurrent access |
| File serving | ezShare-compatible, ~500 KB/s |
| Cloud push | HTTPS, 32 KB chunks, sync handshake |
| OTA | Yes |
| CPAP fit | Direct insertion (no ribbon cable) |
| Power | From CPAP 3.3V rail |
| Status | Production firmware (hms-sleeplink-firmware), deployed |

**Best for:** Production SleepLink device. Reliable, fast, fits directly.

## Option 2: FYSETC SD-WiFi v2.1 ($9)

**Board:** ESP8285H16, 2 MB flash, 80 KB RAM, no MUX
**Firmware:** hms-cpap-arduino (Arduino)
**Form factor:** SD card (needs ribbon cable for CPAP)

| Aspect | Details |
|--------|---------|
| Bus sharing | Software tri-state + CS_SENSE interrupt — unreliable during therapy |
| File serving | ezShare-compatible, ~137 KB/s |
| Cloud push | HTTP only (HTTPS crashes), 4 KB chunks |
| OTA | No (2 MB flash too small) |
| CPAP fit | Needs SD extender ribbon cable |
| Power | From CPAP 3.3V rail (via ribbon) |
| Status | v2026.1.0, working file server, push mode needs HTTP endpoint |

**Best for:** Budget users who only need file serving when CPAP is off.

## Option 3: ezShare WiFi SD + ESP32-C3 Bridge ($35-48)

**Board:** ezShare WiFi SD card + separate ESP32-C3 SuperMini
**Firmware:** sleeplink-push-c3 (ESP-IDF, dual-chip SPI)
**Form factor:** Two devices (ezShare in CPAP, C3 nearby)

| Aspect | Details |
|--------|---------|
| Bus sharing | ezShare handles it (proprietary hardware) |
| File serving | ezShare built-in (phone connects to ezShare AP) |
| Cloud push | C3 reads from ezShare over WiFi, pushes to cloud |
| OTA | C3 supports OTA |
| CPAP fit | ezShare fits directly; C3 is external |
| Power | ezShare from CPAP; C3 needs USB power |
| Status | v3.2.0 with sync handshake, working |

**Best for:** Users who already own an ezShare card.
**Discontinued rationale:** ezShare costs $20-45, plus C3 ($3-15), total $35-48. More expensive and janky than the Pro.

## Option 4: Raspberry Pi (Pi Zero 2 W / Pi 3/4)

**Board:** Any Raspberry Pi with WiFi
**Firmware:** hms-cpap (C++ systemd service)
**Form factor:** Separate device with USB WiFi dongle connecting to ezShare AP

| Aspect | Details |
|--------|---------|
| Bus sharing | Not applicable (reads from ezShare over WiFi) |
| File serving | hms-cpap web UI with charts |
| Cloud push | Via hms-cpap (full featured) |
| OTA | Yes (standard Linux updates) |
| CPAP fit | External device |
| Power | USB power supply |
| Status | Legacy approach, being replaced by SD-card-based solutions |

**Best for:** Users who want full local processing (charts, ML analysis).

## Cost Comparison

| Option | Hardware Cost | Experience | Reliability |
|--------|-------------|------------|-------------|
| Fysetc Pro | $28 | Plug-and-go | High (MUX) |
| Fysetc v2.1 | $9 + ribbon ($5) | Ribbon cable required | Medium (no MUX) |
| ezShare + C3 | $35-48 | Two devices | Medium |
| Raspberry Pi | $15-80 + accessories | External box | High |

## Recommendation

**For SleepLink product:** FYSETC SD WiFi Pro ($28). Direct fit, hardware MUX, HTTPS, OTA. The $19 premium over the v2.1 buys real reliability and a better user experience.

**For budget/DIY users:** FYSETC SD-WiFi v2.1 ($9) with hms-cpap-arduino. Works for file serving when CPAP is off. Requires ribbon cable and has limitations.

**For self-hosted power users:** Raspberry Pi running hms-cpap. Full local control, charts, ML, no cloud dependency.
