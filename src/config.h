#pragma once

// =============================================================================
// sleeplink-firmware-lite — ESP8285 Dual-Mode CPAP Bridge
// Port of hms-cpap-fysetc (ESP-IDF) to Arduino
// =============================================================================

// -- Hardware Pins (FYSETC SD-WiFi v2.1) --
#define SD_CS_PIN           4
#define MISO_PIN            12
#define MOSI_PIN            13
#define SCLK_PIN            14
#define CS_SENSE_PIN        5   // Interrupt: external master SPI activity
#define BOOT_BUTTON_PIN     0
#define LED_PIN             2   // Active LOW

// -- SD Card --
#define SD_SPI_SPEED        SD_SCK_MHZ(4)  // Low speed for ribbon cable + shared bus
#define SD_MOUNT_PATH       ""  // SdFat uses relative paths

// -- File Server (AP mode) --
#define FILE_SERVER_PORT    80
#define FILE_CHUNK_BYTES    1460  // ESP8266 TCP MSS

// -- Cloud Push (STA mode) --
#define PUSH_CHUNK_SIZE     4096  // Max upload chunk (RAM constrained)
#define PUSH_INTERVAL_SEC   65    // Seconds between push cycles
#define HTTP_TIMEOUT_MS     15000
#define HTTP_MAX_RETRIES    3
#define RESP_BUF_SIZE       1024  // Sync response buffer
#define DEFAULT_API_URL     "https://api.sleeplinkusa.com"

// -- Device Identity (baked at compile time) --
#define DEVICE_SECRET       ""    // Set via build_flags: -DDEVICE_SECRET=\"...\"

// -- Operating Modes --
#define OP_MODE_FILE_SERVER 0
#define OP_MODE_CLOUD_PUSH  1

// -- File Scanner --
#define MAX_PENDING_FILES   16
#define DATALOG_DIR         "DATALOG"
#define STR_FILENAME        "STR.edf"

// -- EEPROM Layout --
#define EEPROM_SIZE         512
#define EEPROM_CFG_OFFSET   0     // Config struct
#define EEPROM_CKPT_OFFSET  280   // File checkpoints
#define MAX_CHECKPOINTS     4

// -- WiFi --
#define WIFI_AP_NAME        "SleepLink"  // Will append MAC suffix
#define WIFI_CONFIG_TIMEOUT 180          // Captive portal timeout (seconds)

// -- Firmware --
#ifndef APP_VERSION
#define APP_VERSION         "1.0.0"
#endif
