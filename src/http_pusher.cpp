#include "http_pusher.h"
#include "sd_card.h"
#include "nvs_store.h"
#include "config.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// Port of hms-cpap-fysetc/main/http_pusher.c
// ESP8266HTTPClient instead of esp_http_client
// ArduinoJson instead of cJSON
// 4KB chunks instead of 32KB

static char s_api_url[128];
static char s_serial[33];
static char s_secret[65];

static uint8_t s_chunk_buf[PUSH_CHUNK_SIZE];

void http_pusher_init(const char *api_url, const char *serial, const char *secret) {
    strncpy(s_api_url, api_url, sizeof(s_api_url) - 1);
    strncpy(s_serial, serial, sizeof(s_serial) - 1);
    strncpy(s_secret, secret, sizeof(s_secret) - 1);
    Serial.printf("[pusher] Init: api=%s serial=%s\n", s_api_url, s_serial);
}

// ---------------------------------------------------------------------------
// HTTP client helper — creates WiFiClient based on URL scheme
// ---------------------------------------------------------------------------
static WiFiClient *create_client(const char *url) {
    if (strncmp(url, "https", 5) == 0) {
        auto *client = new BearSSL::WiFiClientSecure();
        client->setInsecure();  // Skip cert verification (ESP8266 RAM constraint)
        return client;
    }
    return new WiFiClient();
}

// ---------------------------------------------------------------------------
// POST binary with metadata headers. Retries with exponential backoff.
// ---------------------------------------------------------------------------
static bool post_binary(const char *path, const uint8_t *body, int body_len,
                        const char *date_folder, const char *filename,
                        uint32_t offset) {
    char url[256];
    snprintf(url, sizeof(url), "%s%s", s_api_url, path);

    char offset_str[16];
    snprintf(offset_str, sizeof(offset_str), "%lu", (unsigned long)offset);

    int delays[] = {1000, 5000, 15000};

    for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
        WiFiClient *client = create_client(url);
        HTTPClient http;

        if (!http.begin(*client, url)) {
            delete client;
            Serial.printf("[pusher] begin failed for %s\n", path);
            continue;
        }

        http.addHeader("Content-Type", "application/octet-stream");
        http.addHeader("X-Device-Serial", s_serial);
        http.addHeader("X-Device-Secret", s_secret);
        if (date_folder) http.addHeader("X-Date-Folder", date_folder);
        if (filename) http.addHeader("X-Filename", filename);
        http.addHeader("X-Offset", offset_str);
        http.setTimeout(HTTP_TIMEOUT_MS);

        int status = http.POST(body, body_len);
        http.end();
        delete client;

        if (status >= 200 && status < 300) return true;

        Serial.printf("[pusher] POST %s failed (attempt %d, status %d)\n",
                      path, attempt + 1, status);

        if (attempt < HTTP_MAX_RETRIES - 1) delay(delays[attempt]);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Upload an EDF file in 4KB chunks
// ---------------------------------------------------------------------------
bool http_pusher_upload_edf(const pending_file_t *file) {
    char rel_path[80];
    snprintf(rel_path, sizeof(rel_path), "%s/%s/%s",
             DATALOG_DIR, file->date_folder, file->filename);

    SdFat &sd = sd_card_get();
    FatFile f;
    if (!f.open(&sd, rel_path, O_RDONLY)) {
        Serial.printf("[pusher] Cannot open %s\n", rel_path);
        return false;
    }

    if (file->offset > 0) f.seekSet(file->offset);

    uint32_t remaining = file->size - file->offset;
    bool success = true;
    uint32_t sent = 0;

    while (remaining > 0) {
        uint32_t chunk = remaining > PUSH_CHUNK_SIZE ? PUSH_CHUNK_SIZE : remaining;
        int n = f.read(s_chunk_buf, chunk);
        if (n <= 0) break;

        if (!post_binary("/api/v1/data/edf", s_chunk_buf, n,
                         file->date_folder, file->filename,
                         file->offset + sent)) {
            success = false;
            break;
        }
        sent += n;
        remaining -= n;
        yield();
    }

    f.close();

    if (success) {
        Serial.printf("[pusher] Uploaded %s/%s (%lu bytes from offset %lu)\n",
                      file->date_folder, file->filename,
                      (unsigned long)sent, (unsigned long)file->offset);
    }
    return success;
}

// ---------------------------------------------------------------------------
// Upload STR.edf
// ---------------------------------------------------------------------------
bool http_pusher_upload_str(uint32_t offset, uint32_t size) {
    SdFat &sd = sd_card_get();
    FatFile f;
    if (!f.open(&sd, STR_FILENAME, O_RDONLY)) return false;

    if (offset > 0) f.seekSet(offset);

    uint32_t to_send = size - offset;
    bool success = true;
    uint32_t sent = 0;

    while (sent < to_send) {
        uint32_t chunk = (to_send - sent) > PUSH_CHUNK_SIZE ? PUSH_CHUNK_SIZE : (to_send - sent);
        int n = f.read(s_chunk_buf, chunk);
        if (n <= 0) break;

        if (!post_binary("/api/v1/data/str", s_chunk_buf, n, NULL, NULL, offset + sent)) {
            success = false;
            break;
        }
        sent += n;
        yield();
    }

    f.close();

    if (success) {
        Serial.printf("[pusher] Uploaded STR.edf (%lu bytes from offset %lu)\n",
                      (unsigned long)sent, (unsigned long)offset);
    }
    return success;
}

// ---------------------------------------------------------------------------
// Heartbeat — periodic check-in + remote reset detection
// ---------------------------------------------------------------------------
bool http_pusher_heartbeat() {
    char json[256];
    snprintf(json, sizeof(json),
             "{\"uptime_s\":%lu,\"rssi\":%d,\"free_heap\":%u,\"fw_version\":\"%s\"}",
             millis() / 1000,
             WiFi.RSSI(),
             ESP.getFreeHeap(),
             APP_VERSION);

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/device/heartbeat", s_api_url);

    WiFiClient *client = create_client(url);
    HTTPClient http;

    if (!http.begin(*client, url)) {
        delete client;
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Serial", s_serial);
    http.addHeader("X-Device-Secret", s_secret);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int status = http.POST((uint8_t *)json, strlen(json));
    String response = http.getString();
    http.end();
    delete client;

    if (status < 200 || status >= 300) return false;

    // Check for remote reset command
    if (response.indexOf("\"reset\"") >= 0 && response.indexOf("true") >= 0) {
        Serial.println(F("[pusher] Remote reset requested"));
        nvs_store_clear_wifi();
        delay(500);
        ESP.restart();
    }

    return true;
}

// ---------------------------------------------------------------------------
// Sync handshake — ask cloud what it already has
// Port of hms-cpap-fysetc v0.6.3 sync
// ---------------------------------------------------------------------------
bool http_pusher_sync(scan_result_t *scan) {
    if (scan->count == 0 && !scan->str_changed) return true;

    // Build manifest JSON
    StaticJsonDocument<1024> doc;
    JsonArray files = doc.createNestedArray("files");

    for (int i = 0; i < scan->count; i++) {
        pending_file_t &pf = scan->files[i];
        JsonObject entry = files.createNestedObject();
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", pf.date_folder, pf.filename);
        entry["path"] = path;
        entry["size"] = pf.size;
        entry["offset"] = pf.offset;
    }

    if (scan->str_changed) {
        doc["str_size"] = scan->str_size;
        doc["str_offset"] = scan->str_offset;
    }

    char body[1024];
    serializeJson(doc, body, sizeof(body));

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/data/sync", s_api_url);

    WiFiClient *client = create_client(url);
    HTTPClient http;

    if (!http.begin(*client, url)) {
        delete client;
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Serial", s_serial);
    http.addHeader("X-Device-Secret", s_secret);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int status = http.POST((uint8_t *)body, strlen(body));
    String response = (status >= 200 && status < 300) ? http.getString() : "";
    http.end();
    delete client;

    if (status < 200 || status >= 300) {
        Serial.printf("[pusher] Sync failed (status %d), uploading all\n", status);
        return false;
    }

    // Parse response
    StaticJsonDocument<1024> resp;
    if (deserializeJson(resp, response)) {
        Serial.println(F("[pusher] Sync response parse failed, uploading all"));
        return false;
    }

    int synced = 0;

    JsonArray resp_files = resp["files"];
    for (JsonObject item : resp_files) {
        const char *path = item["path"];
        uint32_t cloud_offset = item["offset"];
        if (!path) continue;

        for (int i = 0; i < scan->count; i++) {
            pending_file_t &pf = scan->files[i];
            char pf_path[64];
            snprintf(pf_path, sizeof(pf_path), "%s/%s", pf.date_folder, pf.filename);
            if (strcmp(pf_path, path) != 0) continue;

            if (cloud_offset >= pf.size) {
                nvs_store_set_checkpoint(pf.date_folder, pf.filename, pf.size);
                pf.size = 0;  // Mark as skip
                synced++;
            } else if (cloud_offset > pf.offset) {
                nvs_store_set_checkpoint(pf.date_folder, pf.filename, cloud_offset);
                pf.offset = cloud_offset;
                synced++;
            }
            break;
        }
    }

    // Handle STR sync
    if (scan->str_changed && resp.containsKey("str_offset")) {
        uint32_t cloud_str = resp["str_offset"];
        if (cloud_str >= scan->str_size) {
            nvs_store_set_str_size(scan->str_size);
            scan->str_changed = false;
            synced++;
        } else if (cloud_str > scan->str_offset) {
            nvs_store_set_str_size(cloud_str);
            scan->str_offset = cloud_str;
            synced++;
        }
    }

    // Compact — remove fully synced files
    int write_idx = 0;
    for (int i = 0; i < scan->count; i++) {
        if (scan->files[i].size > 0) {
            if (write_idx != i) scan->files[write_idx] = scan->files[i];
            write_idx++;
        }
    }
    scan->count = write_idx;

    Serial.printf("[pusher] Sync: %d on cloud, %d remaining\n",
                  synced, scan->count + (scan->str_changed ? 1 : 0));
    return true;
}
