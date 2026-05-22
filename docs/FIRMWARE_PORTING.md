# Firmware Porting: ESP-IDF to Arduino

## Overview

hms-cpap-arduino is a port of [hms-fysetc](https://github.com/hms-homelab/hms-fysetc) (ESP-IDF, C) to Arduino (C++). This document captures the API translation decisions and gotchas encountered during the port.

## File-by-File Mapping

| hms-cpap-fysetc (ESP-IDF) | hms-cpap-arduino (Arduino) | Notes |
|---------------------------|---------------------------|-------|
| `main/main.c` | `src/main.cpp` | `app_main()` -> `setup()`/`loop()` |
| `main/file_server.c` | `src/file_server.cpp` | `httpd_*` -> `ESP8266WebServer` |
| `main/http_pusher.c` | `src/http_pusher.cpp` | `esp_http_client` -> `ESP8266HTTPClient` |
| `main/file_scanner.c` | `src/file_scanner.cpp` | `dirent.h` -> SdFat `FatFile::openNext()` |
| `main/nvs_store.c` | `src/nvs_store.cpp` | NVS flash -> `EEPROM` (512 bytes) |
| `main/sd_manager.c` | `src/sd_card.cpp` | SDMMC/VFS -> SdFat SPI |
| `main/wifi_manager.c` + `captive_portal.c` | WiFiManager library | Custom portal -> library portal |
| `main/bus_arbiter.c` | (in sd_card.cpp) | PCNT -> CS_SENSE GPIO interrupt |
| `main/traffic_monitor.c` | N/A | No PCNT on ESP8266 |
| `main/fsm.c` | (in main.cpp) | State machine simplified to if/else |
| `main/config.h` | `src/config.h` | Kconfig -> `#define` constants |

## API Translation Reference

### HTTP Server

```
ESP-IDF                              Arduino
-------                              -------
httpd_start(&server, &config)        ESP8266WebServer server(80); server.begin()
httpd_register_uri_handler(s, &uri)  server.on("/path", HTTP_GET, handler)
httpd_req_get_url_query_str(req,...) server.arg("param")
httpd_resp_set_type(req, "text/html") (set in server.send())
httpd_resp_sendstr_chunk(req, str)   server.sendContent(str)
httpd_resp_send_chunk(req, buf, n)   server.sendContent_P(buf, n)
httpd_resp_sendstr_chunk(req, NULL)  server.sendContent("") // end chunked
httpd_resp_send(req, body, len)      server.send(200, "type", body)
httpd_req_get_hdr_value_str(...)     server.header("Range")
                                     server.collectHeaders("Range") // must call first
```

### HTTP Client

```
ESP-IDF                              Arduino
-------                              -------
esp_http_client_init(&cfg)           HTTPClient http; http.begin(client, url)
esp_http_client_set_header(c,k,v)    http.addHeader(key, value)
esp_http_client_set_post_field(...)  (passed to http.POST(body, len))
esp_http_client_perform(client)      int status = http.POST(body, len)
esp_http_client_get_status_code(c)   (returned by http.POST())
esp_http_client_cleanup(client)      http.end(); delete client
event_handler for response capture   String resp = http.getString()
```

### JSON

```
ESP-IDF (cJSON)                      Arduino (ArduinoJson)
-----------                          --------
cJSON_CreateObject()                 StaticJsonDocument<1024> doc
cJSON_AddArrayToObject(root,"files") JsonArray files = doc.createNestedArray("files")
cJSON_AddStringToObject(e,"k","v")   entry["key"] = "value"
cJSON_PrintUnformatted(root)         serializeJson(doc, buf, sizeof(buf))
cJSON_Parse(str)                     deserializeJson(resp, str)
cJSON_GetObjectItem(obj, "key")      resp["key"]
cJSON_IsArray(item)                  (implicit with JsonArray iteration)
cJSON_ArrayForEach(item, arr)        for (JsonObject item : arr)
cJSON_Delete(root)                   (automatic, StaticJsonDocument on stack)
free(body)                           (automatic)
```

### File System

```
ESP-IDF (VFS + dirent)               Arduino (SdFat)
------------------                    --------
opendir("/sdcard/DATALOG")            FatFile dir; dir.open(&sd, "DATALOG", O_RDONLY)
readdir(dir) -> d_name                FatFile entry; entry.openNext(&dir, O_RDONLY)
                                      entry.getName(buf, sizeof(buf))
stat(path, &st) -> st.st_size         entry.fileSize()
S_ISDIR(st.st_mode)                   entry.isDir()
closedir(dir)                         dir.close()
fopen(path, "rb")                     FatFile f; f.open(&sd, path, O_RDONLY)
fread(buf, 1, n, f)                   f.read(buf, n)
fseek(f, offset, SEEK_SET)            f.seekSet(offset)
fclose(f)                             f.close()
```

### Storage

```
ESP-IDF (NVS)                         Arduino (EEPROM)
---------                             -------
nvs_flash_init()                      EEPROM.begin(512)
nvs_open("ns", NVS_READWRITE, &h)    (direct EEPROM access by offset)
nvs_get_u32(h, "key", &val)           EEPROM.get(offset, val)
nvs_set_u32(h, "key", val)            EEPROM.put(offset, val)
nvs_commit(h)                         EEPROM.commit()
nvs_close(h)                          (no close needed)
nvs_erase_all(h)                      (loop EEPROM.write(i, 0))
```

### WiFi

```
ESP-IDF                               Arduino
-------                               -------
esp_wifi_init(&cfg)                   (automatic with WiFi.begin())
esp_wifi_set_mode(WIFI_MODE_STA)      WiFi.mode(WIFI_STA)
esp_wifi_set_config(IF_STA, &cfg)     WiFi.begin(ssid, pass)
esp_wifi_sta_get_ap_info(&ap)         WiFi.RSSI()
esp_wifi_scan_start(NULL, true)       WiFi.scanNetworks()
esp_restart()                         ESP.restart()
esp_get_free_heap_size()              ESP.getFreeHeap()
esp_timer_get_time()                  millis()
vTaskDelay(pdMS_TO_TICKS(ms))         delay(ms)
ESP_LOGI(TAG, fmt, ...)               Serial.printf("[tag] " fmt "\n", ...)
```

## Gotchas

### 1. DEDICATED_SPI vs SHARED_SPI

Using `DEDICATED_SPI` in `SdSpiConfig` causes `sd.begin()` to fail on second call after `SPI.end()`. The SPI peripheral doesn't re-initialize properly.

**Fix:** Use `SHARED_SPI`. SdFat then calls `SPI.beginTransaction()`/`SPI.endTransaction()` per operation, allowing clean re-init.

### 2. ESP8266WebServer Chunked Transfer Leaks Memory

When using `CONTENT_LENGTH_UNKNOWN`, ESP8266WebServer internally accumulates state for each chunk sent. For large files (>1 MB), this exhausts heap and crashes.

**Fix:** Calculate file size with `file.fileSize()`, call `server.setContentLength(size)` before sending, and use `sendContent_P()` for zero-copy chunk sending.

### 3. WiFiManager Remembers Credentials Across Reflash

WiFiManager stores WiFi credentials in a separate flash sector (SDK wifi config area), not in the sketch. Reflashing the firmware does NOT clear saved WiFi credentials. The device will auto-connect to the last configured network after reflash.

To clear: hold FLASH button for 5 seconds (firmware reset), or call `wm.resetSettings()` in code.

### 4. ArduinoJson StaticJsonDocument Size

`StaticJsonDocument<N>` allocates N bytes on the stack. With the ESP8266's 4 KB default stack, a `StaticJsonDocument<1024>` is safe but `<2048>` risks stack overflow when called from nested functions.

For sync response parsing, 1024 bytes limits us to ~16 files in the manifest. This matches `MAX_PENDING_FILES = 16`.

### 5. yield() is Critical

The ESP8266 runs WiFi in the background via `yield()` calls. Any loop that runs for more than ~20ms without yielding will trigger a watchdog reset. File download loops and SD scanning loops must call `yield()` after each chunk/entry.

### 6. sendContent_P vs sendContent

`sendContent(const char* buf, size_t len)` makes a copy via String. `sendContent_P(const char* buf, size_t len)` sends from program memory without copying. Despite the "P" (PROGMEM) suffix, it works with RAM buffers too and avoids the String allocation.

### 7. WiFiClientSecure Heap Usage

`BearSSL::WiFiClientSecure` requires ~15 KB for TLS handshake. On ESP8285 with ~37 KB free heap, this leaves only ~22 KB for everything else. Combined with HTTP client buffers and file read buffers, the heap fragments and crashes.

**Status:** HTTPS push is not reliable on ESP8285. Use HTTP or a local TLS-terminating proxy.
