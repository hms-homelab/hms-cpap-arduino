# CPAP SD Card Error Investigation (2026-04-08)

## Problem

When the FYSETC SD-WiFi v2.1 (ESP8285) board is plugged into the ResMed
CPAP's SD slot and powered on, the CPAP throws an SD card error. No HTTP
requests were made to the ESP — the error occurs during the boot sequence
before any SD bus access by the firmware.

## What we confirmed

1. **File server works in isolation.** With the board on USB power (not in
   the CPAP), all 3 endpoints work back-to-back without crashing:
   - `GET /dir?dir=DATALOG` — folder listing (200 OK)
   - `GET /download` small file — 8 bytes, 0.16s (200 OK)
   - `GET /download` larger file — 7,026 bytes, 0.11s (200 OK)
   - The `client().write()` fix (commit 559f78d) resolved the prior
     download crash caused by `sendContent_P()` treating RAM as PROGMEM.

2. **No MUX on this board.** Verified from the V2.0 schematic
   (`SD-WFI V2.0.pdf`). The ESP-M2 module's HSPI pins connect directly
   to the SD card header via traces — no analog switch or buffer IC.
   - GPIO14 (HSCLK) → SD_CLK
   - GPIO12 (HMISO) → SD_D0
   - GPIO13 (HMOSI) → SD_CMD
   - GPIO4 → SD_CS
   Only other ICs: AP2112 (3.3V LDO), CH340E (USB-serial), ESP-M2 module.

3. **The FYSETC SD-WiFi Pro (ESP32-PICO) has a hardware MUX** on GPIO26
   that physically disconnects the ESP from the SD bus. That's why the
   FYSETC C3 firmware (hms-cpap-fysetc) doesn't have this problem — it
   uses a board with hardware bus isolation.

4. **The CPAP error is NOT caused by firmware SD access.** No `sd_card_mount()`
   is called until an HTTP request arrives. The pins are tri-stated via
   `sd_card_init()` → `pinMode(pin, INPUT)` at boot. The error happens
   before any request reaches the web server.

## Leading hypothesis

The ESP8285's GPIO pads present parasitic capacitive load (~5pF per pin)
on the SD bus even in INPUT mode. With 4 SPI pins (MISO, MOSI, SCLK, CS)
that's ~20pF of extra bus capacitance. The CPAP's SD controller may be
sensitive to this — either:

- Signal integrity degradation (slower edges, ringing)
- The ESP8266 Arduino `pinMode(INPUT)` doesn't fully disconnect the
  output driver or leaves weak internal pull-ups/pull-downs active
- The WiFi stack or SPI library briefly reconfigures HSPI pins during
  `WiFiManager::autoConnect()`, momentarily driving the bus

The ESPWebDAV firmware works fine on 3D printers because Marlin only
reads the SD card on user demand, not continuously. The CPAP polls the
SD card constantly.

## Next steps: debug firmware (commit a76443b)

Added timestamped serial prints and pauses at each boot stage:

```
[main] === BOOT t=500 ===
[main] [T] sd_card_init (tri-state pins)...
[main] [T] sd_card_init DONE — pins should be high-Z
[main] [T] >>> PAUSE 5s — check CPAP SD status NOW <<<    ← window 1
[main] [T] WiFi autoConnect starting...
[main] [T] WiFi autoConnect done
[main] [T] >>> PAUSE 3s — check CPAP SD status NOW <<<    ← window 2
[main] [T] File Server mode — starting web server...
[main] [T] Web server READY
```

### Test procedure

1. Plug ESP board into CPAP SD slot
2. Connect USB serial at 115200 baud (`screen /dev/cu.wchusbserial1410 115200`)
3. Power on CPAP
4. Watch serial output — note the timestamp when the CPAP throws the SD error
5. The two 5s/3s pauses give windows to check if the CPAP is happy:
   - **Window 1 (after sd_card_init):** If CPAP is OK here, the tri-state
     is working and the problem is later (WiFi, web server, etc.)
   - **Window 2 (after WiFi connect):** If CPAP dies here but was OK at
     window 1, then `autoConnect()` is stomping on the SPI pins

### Possible outcomes

| CPAP fails at | Cause | Fix |
|---------------|-------|-----|
| Before boot (instant) | Electrical — parasitic capacitance | Hardware only (MUX or series resistors) |
| Window 1 (after sd_card_init) | `pinMode(INPUT)` not truly high-Z | Try register-level GPIO disconnect, `SPI.end()` |
| Window 2 (after WiFi) | WiFi stack reconfigures HSPI pins | Explicitly re-tri-state pins after `autoConnect()` |
| After web server start | Something in `ESP8266WebServer` | Investigate server init |
| Only on first HTTP request | `sd_card_mount()` / `sd.begin()` contention | Normal bus sharing issue (see BUS_SHARING.md) |

## Hardware options if software can't fix it

See SLEEPLINK_HARDWARE_OPTIONS.md. If the v2.1 can't coexist with the
CPAP electrically, the cheapest path is the SD-WiFi Pro (~$20 more) which
has the MUX built in. Adding an external MUX to the v2.1 defeats the
cost advantage.
