#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "config.h"
#include "nvs_store.h"
#include "sd_card.h"
#include "file_server.h"
#include "file_scanner.h"
#include "http_pusher.h"

// =============================================================================
// hms-cpap-arduino — ESP8285 Dual-Mode CPAP Bridge
//
// Mode 0 (File Server): ezShare-compatible HTTP file server on port 80
// Mode 1 (Cloud Push):  Sync + chunked EDF upload to CpapDash API
// =============================================================================

static DeviceConfig g_config;
static ESP8266WebServer g_web_server(FILE_SERVER_PORT);
static WiFiManager g_wm;
static WiFiUDP g_ntp_udp;
static NTPClient g_ntp(g_ntp_udp, "pool.ntp.org", 0, 60000);

// Mode is hardcoded to File Server. Cloud Push is not viable on ESP8285:
// BearSSL handshake + WiFi stack exhausts the ~40KB free heap during chunked
// HTTPS uploads. See docs/ESP8285_CONSTRAINTS.md.
// (Captive portal mode picker removed.)

static unsigned long g_last_push = 0;
static uint8_t g_mode = OP_MODE_FILE_SERVER;
static bool g_wifi_connected = false;

// Device serial derived from chip ID
static char g_serial[24];

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
static void led_on()  { digitalWrite(LED_PIN, LOW); }
static void led_off() { digitalWrite(LED_PIN, HIGH); }
static void led_blink(int ms) { led_on(); delay(ms); led_off(); }

// ---------------------------------------------------------------------------
// WiFiManager save callback
// ---------------------------------------------------------------------------
static void save_config_callback() {
    // WiFiManager handles WiFi creds internally
    // We just need to save the mode from the form
    // WiFiManager doesn't parse custom HTML radio buttons, so we
    // read it from the HTTP server post-save via the web server params
    Serial.println(F("[main] WiFi config saved"));
}

// ---------------------------------------------------------------------------
// Boot button — hold 5s to reset WiFi + config
// ---------------------------------------------------------------------------
static void check_boot_button() {
    static unsigned long press_start = 0;
    static bool was_pressed = false;

    bool pressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);

    if (pressed && !was_pressed) {
        press_start = millis();
    } else if (pressed && was_pressed && (millis() - press_start > 5000)) {
        Serial.println(F("[main] Boot button held 5s — resetting"));
        for (int i = 0; i < 6; i++) { led_blink(100); delay(100); }
        g_wm.resetSettings();
        nvs_store_clear_wifi();
        nvs_store_set_mode(OP_MODE_FILE_SERVER);
        delay(500);
        ESP.restart();
    }

    was_pressed = pressed;
}

// ---------------------------------------------------------------------------
// Push cycle (Cloud Push mode) — scan + sync + upload + heartbeat
// ---------------------------------------------------------------------------
static void push_cycle() {
    Serial.printf("[main] Push cycle (heap=%u)\n", ESP.getFreeHeap());

    if (!sd_card_mount()) {
        Serial.println(F("[main] SD mount failed, skipping push"));
        return;
    }

    scan_result_t scan = file_scanner_scan();

    if (scan.count == 0 && !scan.str_changed) {
        sd_card_unmount();
        http_pusher_heartbeat();
        return;
    }

    http_pusher_sync(&scan);

    for (int i = 0; i < scan.count; i++) {
        pending_file_t &pf = scan.files[i];
        if (pf.size == 0) continue;

        if (http_pusher_upload_edf(&pf)) {
            nvs_store_set_checkpoint(pf.date_folder, pf.filename, pf.size);
        }
        yield();
    }

    if (scan.str_changed) {
        if (http_pusher_upload_str(scan.str_offset, scan.str_size)) {
            nvs_store_set_str_size(scan.str_size);
        }
    }

    sd_card_unmount();
    http_pusher_heartbeat();

    Serial.printf("[main] Push cycle done (heap=%u)\n", ESP.getFreeHeap());
}

// =============================================================================
// setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n\n[main] === BOOT t=%lu ===\n", millis());
    Serial.printf("[main] CpapDash Firmware Lite %s\n", APP_VERSION);
    Serial.printf("[main] Chip: ESP8285, Flash: %uKB, Free heap: %u\n",
                  ESP.getFlashChipRealSize() / 1024, ESP.getFreeHeap());

    // Generate serial from chip ID
    snprintf(g_serial, sizeof(g_serial), "CD-%08X", ESP.getChipId());
    Serial.printf("[main] Device serial: %s\n", g_serial);

    // LED
    pinMode(LED_PIN, OUTPUT);
    led_off();
    led_blink(200);

    // Boot button
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // EEPROM + config
    nvs_store_init();
    nvs_store_load(g_config);
    g_mode = nvs_store_get_mode();

    Serial.printf("[main] Saved mode: %s\n",
                  g_mode == OP_MODE_CLOUD_PUSH ? "Cloud Push" : "File Server");

    // SD card — tri-state all SPI pins
    Serial.printf("[main] [%lu] sd_card_init (tri-state pins)...\n", millis());
    sd_card_init();
    Serial.printf("[main] [%lu] sd_card_init DONE — pins should be high-Z\n", millis());

    Serial.printf("[main] [%lu] >>> PAUSE 5s — check CPAP SD status NOW <<<\n", millis());
    for (int i = 5; i > 0; i--) {
        Serial.printf("[main] [%lu] ...%d\n", millis(), i);
        delay(1000);
    }

    // WiFiManager setup — File Server mode only; no mode picker
    Serial.printf("[main] [%lu] WiFiManager setup...\n", millis());
    g_wm.setSaveConfigCallback(save_config_callback);
    g_wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
    g_wm.setDarkMode(true);
    // Force File Server mode in NVS on every boot
    nvs_store_set_mode(OP_MODE_FILE_SERVER);
    g_mode = OP_MODE_FILE_SERVER;

    // Generate AP name with chip ID suffix
    char ap_name[32];
    snprintf(ap_name, sizeof(ap_name), "%s-%04X",
             WIFI_AP_NAME, (uint16_t)(ESP.getChipId() & 0xFFFF));

    // Try to connect
    Serial.printf("[main] [%lu] WiFi autoConnect starting...\n", millis());
    g_wifi_connected = g_wm.autoConnect(ap_name);
    Serial.printf("[main] [%lu] WiFi autoConnect done (connected=%d)\n", millis(), g_wifi_connected);

    Serial.printf("[main] [%lu] >>> PAUSE 3s — check CPAP SD status NOW <<<\n", millis());
    for (int i = 3; i > 0; i--) {
        Serial.printf("[main] [%lu] ...%d\n", millis(), i);
        delay(1000);
    }

    if (g_wifi_connected) {
        Serial.printf("[main] Connected to %s (%s)\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

        Serial.printf("[main] [%lu] mDNS begin...\n", millis());
        if (MDNS.begin("cpapdash")) {
            Serial.println(F("[main] mDNS hostname: cpapdash.local"));
        }

        Serial.printf("[main] [%lu] NTP begin...\n", millis());
        g_ntp.begin();
        g_ntp.update();
        Serial.printf("[main] [%lu] NTP done\n", millis());

        if (g_mode == OP_MODE_CLOUD_PUSH) {
            Serial.println(F("[main] Cloud Push mode"));
            http_pusher_init(g_config.api_url, g_serial, DEVICE_SECRET);
            Serial.printf("[main] Push interval: %ds\n", PUSH_INTERVAL_SEC);
        } else {
            Serial.printf("[main] [%lu] File Server mode — starting web server...\n", millis());
            file_server_init(g_web_server);
            g_web_server.begin();
            Serial.printf("[main] [%lu] Web server READY at http://%s/\n",
                          millis(), WiFi.localIP().toString().c_str());
        }
    } else {
        Serial.printf("[main] [%lu] AP mode — file server on 192.168.4.1\n", millis());
        file_server_init(g_web_server);
        g_web_server.begin();
    }

    led_off();
}

// =============================================================================
// loop
// =============================================================================
void loop() {
    check_boot_button();
    MDNS.update();

    if (g_wifi_connected && g_mode == OP_MODE_CLOUD_PUSH) {
        // Cloud Push mode
        if (WiFi.isConnected()) {
            g_ntp.update();

            unsigned long now = millis();
            if (now - g_last_push >= (unsigned long)PUSH_INTERVAL_SEC * 1000 ||
                g_last_push == 0) {
                g_last_push = now;
                led_on();
                push_cycle();
                led_off();
            }
        } else {
            led_blink(50);
            delay(950);
        }
    } else {
        // File Server mode (AP or STA)
        g_web_server.handleClient();
    }

    yield();
}
