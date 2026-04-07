# SPI Bus Sharing: v2.1 vs Pro

## The Problem

The SD-WiFi board plugs into a host device's SD slot (CPAP, 3D printer). Both the ESP MCU and the host device need to access the same micro-SD card. The SD card has a single SPI bus — only one master can drive it at a time.

## FYSETC SD-WiFi v2.1 (ESP8285) — Software Arbitration

The v2.1 has **no hardware bus switch**. The ESP8285 and host device share the same SPI lines with no physical isolation.

### How it Works

1. **Tri-state idle:** ESP8285 SPI pins (MISO/MOSI/SCLK/CS) are configured as INPUT (high-impedance) when not in use. This electrically disconnects the ESP from the bus, allowing the host full access.

2. **CS_SENSE interrupt:** GPIO5 is wired to the host's SD chip select line. An ISR on FALLING edge detects when the host asserts CS to begin an SD transaction. This sets a blockout timer.

3. **Blockout timer:** After detecting host activity, the ESP refuses to touch the bus for N seconds (stock firmware uses 20s, we use 3s). This prevents contention.

4. **Bus acquisition:** When the blockout expires and the ESP needs the bus:
   - Switch MISO/MOSI/SCLK from INPUT to SPECIAL (SPI function)
   - Switch SD_CS to OUTPUT, drive HIGH (deselected)
   - Perform SD operations
   - Tri-state all pins back to INPUT when done

### Code (from ardyesp/ESPWebDAV)

```cpp
void takeBusControl() {
    weHaveBus = true;
    pinMode(MISO, SPECIAL);
    pinMode(MOSI, SPECIAL);
    pinMode(SCLK, SPECIAL);
    pinMode(SD_CS, OUTPUT);
}

void relenquishBusControl() {
    pinMode(MISO, INPUT);
    pinMode(MOSI, INPUT);
    pinMode(SCLK, INPUT);
    pinMode(SD_CS, INPUT);
    weHaveBus = false;
}
```

### Limitations

- **No hardware isolation:** If both sides drive the bus simultaneously, you get signal contention. Data corruption is possible. There is an inherent race between the ISR detecting host activity and the ESP releasing the bus.
- **Slow re-mount:** After tri-stating and re-taking the bus, `sd.begin()` re-initializes the SD card from scratch (~100-300ms). This is too slow for surgical bus steals during active host writes.
- **Workaround:** Initialize `sd.begin()` once, then only tri-state/re-take the pins on subsequent mounts. SdFat's internal state survives the pin toggling if `SPI.end()` is NOT called. Subsequent mounts cost <1ms.
- **CPAP-specific issue:** The ResMed AirSense appears to periodically poll/access the SD card even when therapy is not active. The CS_SENSE approach may not get a clean window on CPAP devices the way it does on 3D printers (Marlin only reads SD on demand).

### Known Issues (3D Printer Community)

- Users report the printer occasionally stops seeing files on SD after WiFi access, requiring card reinsertion or reboot.
- The 20-second blockout is conservative but not foolproof — long print jobs with frequent SD reads can starve the ESP entirely.
- The CS_SENSE interrupt has inherent latency — by the time the ISR fires and the ESP tri-states its pins, the host may already be driving the bus.

## FYSETC SD WiFi Pro (ESP32-PICO-D4) — Hardware MUX

The Pro has a **hardware analog multiplexer (MUX)** IC that physically switches the SD bus between the ESP32 and the host device.

### How it Works

- **GPIO26** controls the MUX switching state (active-low, with pull-up)
- When GPIO26 is in one state: SD bus connects to ESP32
- When in the other state: SD bus connects to host via golden finger connector
- **Hardware-enforced mutual exclusion** — impossible for both sides to drive the bus simultaneously

### Advantages

- Zero contention risk — the MUX physically disconnects one side
- No race conditions
- No ISR-based CS detection needed
- Reliable at any SPI speed
- Can hold the bus for extended operations without corruption risk

### Known Behavior

- When the ESP32 takes the bus for >400ms and then releases it, some USB card readers detect a "capacity change" and remount. This is documented in FYSETC/SD-WIFI-PRO GitHub issue #7.
- The MUX switching time is sub-microsecond — negligible.

## Comparison Table

| Feature | v2.1 (ESP8285) | Pro (ESP32) |
|---------|---------------|-------------|
| Bus isolation | None (software tri-state) | Hardware MUX IC |
| Control pin | GPIO5 (CS_SENSE, INPUT) | GPIO26 (MUX select, OUTPUT) |
| Contention risk | Yes (race conditions) | No (hardware-enforced) |
| Re-mount time | 100-300ms (full sd.begin) or <1ms (pin re-take) | <1ms (MUX switch) |
| Corruption risk | Documented user reports | Eliminated |
| CPAP compatibility | Works when host is idle; unreliable during therapy | Works always (hms-cpap-fysetc bus arbiter) |

## Recommendations

### For File Server Mode (v2.1)

The v2.1 works for serving files **when the CPAP is not actively writing**. Typical use: therapy ends, user downloads session data via phone/browser, then CPAP resumes.

- Keep SdFat initialized after first `sd.begin()`
- Tri-state pins between requests (fast re-take)
- Use `SHARED_SPI` mode so SPI transactions are short
- Lower SPI speed to 4 MHz for signal integrity through ribbon cable
- Accept that concurrent access with CPAP is unreliable

### For Cloud Push Mode (v2.1)

Cloud push should only run when the CPAP is idle:

- Monitor CS_SENSE for a quiet window (no falling edges for 3+ seconds)
- Mount, scan DATALOG, upload changed files, unmount
- If CS_SENSE fires during upload, abort and retry later
- Post-therapy is the natural push window (CPAP finishes writing, releases bus)

### For Reliable Concurrent Access

Use the FYSETC SD WiFi Pro or the hms-sleeplink-firmware (ESP32) which has a proper bus arbiter with PCNT-based traffic monitoring and ISR-driven yield. The v2.1 cannot reliably share the bus during active host writes.

## Sources

- [ardyesp/ESPWebDAV](https://github.com/ardyesp/ESPWebDAV) — original v2.1 firmware with bus sharing code
- [FYSETC/FYSETC-SD-WIFI](https://github.com/FYSETC/FYSETC-SD-WIFI) — v2.x hardware repo
- [FYSETC/SD-WIFI-PRO](https://github.com/FYSETC/SD-WIFI-PRO) — Pro hardware repo with schematic
- [SD-WIFI-PRO Issue #7](https://github.com/FYSETC/SD-WIFI-PRO/issues/7) — MUX remount behavior
- [FYSETC SD-WiFi Pro Wiki](https://wiki.fysetc.com/docs/SD-WiFi-Pro) — GPIO26 MUX documentation
- [esp3d.io SD-WiFi-PRO](https://esp3d.io/ESP3D/Version_3.X/hardware/esp_boards/esp32-pico/sd-wifi-pro/) — confirms MUX design
