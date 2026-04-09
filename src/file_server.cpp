#include "file_server.h"
#include "sd_card.h"
#include "config.h"
#include <Arduino.h>

// Port of hms-cpap-fysetc/main/file_server.c
// ezShare-compatible endpoints: /dir, /download, /api/status

static ESP8266WebServer *s_server = nullptr;

// ---------------------------------------------------------------------------
// URL-decode %5C and normalize backslashes to forward slashes
// ---------------------------------------------------------------------------
static void normalize_path(String &path) {
    path.replace("%5C", "/");
    path.replace("%5c", "/");
    path.replace("\\", "/");
}

// ---------------------------------------------------------------------------
// GET /dir?dir=A:/DATALOG[/YYYYMMDD]
//
// Returns ezShare-style HTML directory listing.
// Format: date/time  size_or_DIR  <a href="...">name</a>
// ---------------------------------------------------------------------------
static void handle_dir() {
    String dir_param = s_server->arg("dir");
    if (dir_param.length() == 0) {
        s_server->send(400, "text/plain", "missing dir param");
        return;
    }

    // Strip "A:" prefix
    if (dir_param.startsWith("A:")) dir_param = dir_param.substring(2);
    normalize_path(dir_param);

    if (dir_param.indexOf("..") >= 0) {
        s_server->send(400, "text/plain", "invalid path");
        return;
    }

    // Remove leading slash for SdFat relative path
    if (dir_param.startsWith("/")) dir_param = dir_param.substring(1);

    if (!sd_card_mount()) {
        s_server->send(500, "text/plain", "sd_mount_failed");
        return;
    }

    SdFat &sd = sd_card_get();
    FatFile dir;
    if (!dir.open(&sd, dir_param.c_str(), O_RDONLY) || !dir.isDir()) {
        sd_card_unmount();
        s_server->send(404, "text/plain", "directory not found");
        return;
    }

    s_server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server->send(200, "text/html", "");
    s_server->sendContent("<html><body><pre>\r\n");

    FatFile entry;
    char name[42];

    while (entry.openNext(&dir, O_RDONLY)) {
        entry.getName(name, sizeof(name));
        if (name[0] == '.') { entry.close(); continue; }

        char line[256];
        if (entry.isDir()) {
            snprintf(line, sizeof(line),
                     "2026- 1- 1    0:00:00         &lt;DIR&gt;   "
                     "<a href=\"dir?dir=A:/%s/%s\"> %s</a>\r\n",
                     dir_param.c_str(), name, name);
        } else {
            long size_kb = (long)((entry.fileSize() + 1023) / 1024);
            if (size_kb < 1) size_kb = 1;
            snprintf(line, sizeof(line),
                     "2026- 1- 1    0:00:00       %5ldKB  "
                     "<a href=\"download?file=/%s/%s\"> %s</a>\r\n",
                     size_kb, dir_param.c_str(), name, name);
        }
        entry.close();
        s_server->sendContent(line);
    }

    dir.close();
    s_server->sendContent("</pre></body></html>");
    s_server->sendContent("");  // End chunked
    sd_card_unmount();

    Serial.printf("[file_srv] GET /dir dir=%s\n", dir_param.c_str());
}

// ---------------------------------------------------------------------------
// GET /download?file=DATALOG/YYYYMMDD/filename.edf
//
// Streams raw file bytes in 1460-byte chunks (ESP8266 MTU).
// Supports Range header for delta downloads.
// ---------------------------------------------------------------------------
static void handle_download() {
    String file_param = s_server->arg("file");
    if (file_param.length() == 0) {
        s_server->send(400, "text/plain", "missing file param");
        return;
    }

    // Strip "A:" prefix
    if (file_param.startsWith("A:")) file_param = file_param.substring(2);
    normalize_path(file_param);

    if (file_param.indexOf("..") >= 0) {
        s_server->send(400, "text/plain", "invalid path");
        return;
    }

    if (file_param.startsWith("/")) file_param = file_param.substring(1);

    // Check for Range header
    long offset = 0;
    if (s_server->hasHeader("Range")) {
        String range = s_server->header("Range");
        int eq = range.indexOf("bytes=");
        if (eq >= 0) {
            offset = range.substring(eq + 6).toInt();
            if (offset < 0) offset = 0;
        }
    }

    if (!sd_card_mount()) {
        s_server->send(500, "text/plain", "sd_mount_failed");
        return;
    }

    SdFat &sd = sd_card_get();
    FatFile file;
    if (!file.open(&sd, file_param.c_str(), O_RDONLY)) {
        sd_card_unmount();
        s_server->send(404, "text/plain", "not found");
        return;
    }

    uint32_t file_size = file.fileSize();
    if (offset > 0) file.seekSet(offset);
    uint32_t remaining = file_size - offset;

    s_server->setContentLength(remaining);
    if (offset > 0) {
        s_server->send(206, "application/octet-stream", "");
    } else {
        s_server->send(200, "application/octet-stream", "");
    }

    uint8_t buf[FILE_CHUNK_BYTES];
    size_t total = 0;
    int n;

            while ((n = file.read(buf, sizeof(buf))) > 0) {

                s_server->client().write(buf, n);

                total += n;

                yield();  // Feed watchdog

            }
    file.close();
    sd_card_unmount();

    Serial.printf("[file_srv] GET /download %s offset=%ld bytes=%zu\n",
                  file_param.c_str(), offset, total);
}

// ---------------------------------------------------------------------------
// GET /api/status
// ---------------------------------------------------------------------------
static void handle_status() {
    char buf[256];
    unsigned long up = millis() / 1000;
    int secs = up % 60, mins = (up / 60) % 60, hrs = up / 3600;

    snprintf(buf, sizeof(buf),
             "{\"state\":\"FILE_SERVER\",\"wifi\":%s,"
             "\"free_heap\":%u,"
             "\"uptime\":\"%dh%02dm%02ds\","
             "\"fw_version\":\"%s\"}",
             WiFi.isConnected() ? "true" : "false",
             ESP.getFreeHeap(),
             hrs, mins, secs,
             APP_VERSION);

    s_server->send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------
// GET / — root redirect
// ---------------------------------------------------------------------------
static void handle_root() {
    s_server->send(200, "text/html",
        "<html><body>"
        "<h2>SleepLink Firmware Lite</h2>"
        "<p><a href=\"/dir?dir=A:/DATALOG\">Browse DATALOG</a></p>"
        "<p><a href=\"/api/status\">Status</a></p>"
        "</body></html>");
}

void file_server_init(ESP8266WebServer &server) {
    s_server = &server;
    server.on("/", HTTP_GET, handle_root);
    server.on("/dir", HTTP_GET, handle_dir);
    server.on("/download", HTTP_GET, handle_download);
    server.on("/api/status", HTTP_GET, handle_status);
    server.collectHeaders("Range");
    Serial.println(F("[file_srv] Registered: / /dir /download /api/status"));
}
