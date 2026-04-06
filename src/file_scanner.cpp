#include "file_scanner.h"
#include "sd_card.h"
#include "nvs_store.h"
#include <Arduino.h>
#include <string.h>

// Port of hms-cpap-fysetc/main/file_scanner.c
// Uses SdFat FatFile instead of dirent.h

static bool is_edf_file(const char *name) {
    size_t len = strlen(name);
    if (len < 5) return false;
    const char *ext = name + len - 4;
    return strcasecmp(ext, ".edf") == 0;
}

static bool is_date_folder(const char *name) {
    if (strlen(name) != 8) return false;
    for (int i = 0; i < 8; i++) {
        if (name[i] < '0' || name[i] > '9') return false;
    }
    return true;
}

scan_result_t file_scanner_scan() {
    scan_result_t result = {};
    SdFat &sd = sd_card_get();

    // Check STR.edf
    FatFile str_file;
    if (str_file.open(&sd, STR_FILENAME, O_RDONLY)) {
        uint32_t cur = str_file.fileSize();
        str_file.close();
        uint32_t prev = nvs_store_get_str_size();
        if (cur > prev) {
            result.str_changed = true;
            result.str_offset = prev;
            result.str_size = cur;
            Serial.printf("[scanner] STR.edf changed: %lu -> %lu\n",
                          (unsigned long)prev, (unsigned long)cur);
        }
    }

    // Scan DATALOG/
    FatFile datalog;
    if (!datalog.open(&sd, DATALOG_DIR, O_RDONLY)) {
        Serial.println(F("[scanner] Cannot open DATALOG/"));
        return result;
    }

    FatFile date_dir;
    char date_name[13];

    while (date_dir.openNext(&datalog, O_RDONLY)) {
        if (!date_dir.isDir()) { date_dir.close(); continue; }

        date_dir.getName(date_name, sizeof(date_name));
        if (!is_date_folder(date_name)) { date_dir.close(); continue; }

        FatFile edf_file;
        char file_name[42];

        while (edf_file.openNext(&date_dir, O_RDONLY)) {
            if (edf_file.isDir()) { edf_file.close(); continue; }

            edf_file.getName(file_name, sizeof(file_name));
            if (!is_edf_file(file_name)) { edf_file.close(); continue; }

            uint32_t cur = edf_file.fileSize();
            edf_file.close();

            uint32_t prev = nvs_store_get_checkpoint(date_name, file_name);

            if (cur > prev && result.count < MAX_PENDING_FILES) {
                pending_file_t &pf = result.files[result.count];
                strncpy(pf.date_folder, date_name, sizeof(pf.date_folder) - 1);
                strncpy(pf.filename, file_name, sizeof(pf.filename) - 1);
                pf.offset = prev;
                pf.size = cur;
                result.count++;
            }
        }
        date_dir.close();
    }
    datalog.close();

    if (result.count > 0) {
        Serial.printf("[scanner] Found %d files with new data\n", result.count);
    }
    return result;
}
