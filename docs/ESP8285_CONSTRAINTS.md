# ESP8285 Constraints and Design Decisions

## Chip Specifications

| Spec | Value | Impact |
|------|-------|--------|
| CPU | Tensilica L106, 80 MHz, single core | No concurrent tasks; WiFi + SPI + HTTP are interleaved |
| RAM | 80 KB SRAM total | ~50 KB free after WiFi stack; all buffers must be small |
| Flash | 2 MB embedded | ~1 MB for sketch after bootloader + WiFi cal; no OTA |
| SPI | Single SPI peripheral | Shared between SD card and WiFi (internally); use SHARED_SPI |
| WiFi | 802.11 b/g/n, 2.4 GHz | ~30 KB RAM consumed by WiFi stack when active |
| SDIO | Not available | SD card access is SPI-only |
| PSRAM | None | No external memory; everything must fit in 80 KB |
| PCNT | Not available | Cannot use pulse counting for bus traffic monitoring |

## Memory Budget

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| WiFi stack (lwIP) | ~30 KB | Fixed, always active when connected |
| Arduino core | ~5 KB | Stack, globals |
| SdFat | ~2 KB | File handles, SPI transaction buffers |
| WiFiManager | ~5 KB | Only during captive portal; freed after config |
| ESP8266WebServer | ~4 KB | Per-request, file server mode |
| ESP8266HTTPClient | ~6 KB | Per-request, push mode |
| Upload chunk buffer | 4 KB | Static `s_chunk_buf[4096]` in http_pusher |
| File scanner result | ~1 KB | 16 x pending_file_t structs |
| EEPROM config | 512 B | Static struct |
| **Total** | **~48 KB** | Leaves ~32 KB free heap at runtime |

### Measured Values

- Free heap at boot (WiFi connected): **40,688 bytes**
- Free heap after file serving: **36,768 bytes**
- Free heap stable after many requests: **~37 KB**

## Key Constraints

### 1. No HTTPS (TLS) for Large Payloads

BearSSL TLS handshake requires ~15 KB of contiguous heap. When combined with the WiFi stack and application buffers, the ESP8285 crashes during HTTPS POST of file chunks.

**Observed:** Exception 2 (illegal instruction) when attempting `https://api.sleeplinkusa.com` push with 4 KB chunk + BearSSL.

**Workaround options:**
- Use HTTP (not HTTPS) for local/dev push endpoints
- Use `setInsecure()` to skip certificate verification (saves some heap but still crashes)
- Reduce chunk size to 2 KB (may work but untested)
- Use MQTT over TLS instead of HTTPS (MQTT has smaller per-message overhead)

**Recommendation:** Cloud push mode on ESP8285 should target an HTTP endpoint or a local proxy. HTTPS is not reliable on this chip with the current memory budget.

### 2. Large File Downloads

Files over ~1.5 MB caused crashes with chunked transfer encoding (`CONTENT_LENGTH_UNKNOWN`). The ESP8266WebServer accumulates internal state per chunk.

**Fix:** Set `Content-Length` header to the actual file size before sending. Use `sendContent_P()` to send from buffer without String allocation. This fixed 2.9 MB BRP file downloads at 137 KB/s.

### 3. SPI Re-initialization

Calling `sd.begin()` after `SPI.end()` on the ESP8285 takes 100-300ms and sometimes fails. The SPI peripheral state is lost when pins are switched to INPUT mode.

**Fix:** Call `sd.begin()` once on first mount. On subsequent mounts, only re-take the SPI pins (pinMode SPECIAL) without calling `sd.begin()` again. SdFat's internal state survives the pin toggling as long as `SPI.end()` is not called.

### 4. No OTA Updates

The 2 MB flash cannot fit two firmware images for OTA (current + new). The sketch uses ~491 KB, and OTA requires at least `sketch_size * 2 + bootloader`. With the WiFi stack and bootloader overhead, there isn't enough room.

**Workaround:** Flash via USB (hold FLASH button, plug in, esptool). The Steigan ESPWebDAV firmware had OTA working by using a compressed `.bin.gz` upload, but the sketch was only 420 KB. Adding ArduinoJson and HTTPClient pushed us past the OTA threshold.

### 5. Single Core / No RTOS

The ESP8285 runs a cooperative multitasking model (Arduino `loop()` + WiFi background). There are no FreeRTOS tasks. All operations must `yield()` periodically to let the WiFi stack process packets.

Long-running operations (file downloads, SD scanning) must call `yield()` in their inner loops to avoid watchdog resets.

### 6. Upload Chunk Size

The ESP-IDF version (hms-cpap-fysetc) uses 32 KB chunks for cloud push. The ESP8285 uses 4 KB chunks due to RAM constraints. This means:

- A 500 KB PLD file = 125 HTTP POST requests (vs 16 on ESP32)
- A 2.9 MB BRP file = 725 HTTP POST requests (vs 91 on ESP32)
- Total upload time is dominated by HTTP round-trip latency, not throughput

For large session uploads, expect 3-5 minutes per session on ESP8285 vs ~30 seconds on ESP32.

## Comparison with ESP32 (FYSETC SD WiFi Pro)

| Feature | ESP8285 (v2.1) | ESP32-PICO-D4 (Pro) |
|---------|---------------|---------------------|
| RAM | 80 KB | 520 KB + 2 MB PSRAM |
| Flash | 2 MB | 4 MB |
| Cores | 1 (80 MHz) | 2 (240 MHz) |
| SPI | 1 (shared) | 2+ (dedicated SDMMC) |
| Bus MUX | None | Hardware analog MUX |
| HTTPS | Unreliable (heap crash) | Works (plenty of RAM) |
| Upload chunk | 4 KB | 32 KB |
| OTA | No | Yes |
| File serve speed | ~137 KB/s | ~500 KB/s |
| PCNT | Not available | Available (bus traffic monitor) |
| Price | ~$9 | ~$28 |

## When to Use Each

**ESP8285 (v2.1) is appropriate when:**
- Budget is primary concern ($9 vs $28)
- File server mode only (browse/download when CPAP is off)
- Cloud push to HTTP endpoint (not HTTPS)
- User is comfortable with ribbon cable workaround for fit
- CPAP bus sharing is not needed (pull files when therapy is done)

**ESP32 (Pro) is appropriate when:**
- Reliability is critical
- Concurrent access with CPAP is needed (bus MUX)
- HTTPS cloud push required
- Faster uploads needed (32 KB chunks)
- OTA updates desired
- Direct SD slot insertion (no ribbon cable)
