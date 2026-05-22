#include "nvs_store.h"
#include "config.h"
#include <EEPROM.h>
#include <string.h>

// EEPROM layout:
//   [0..279]   DeviceConfig
//   [280..283] STR.edf size (uint32_t)
//   [284..459] 4x FileCheckpoint (44 bytes each)

#define MODE_OFFSET      276
#define STR_SIZE_OFFSET  280
#define CKPT_OFFSET      284
#define CKPT_SIZE        sizeof(FileCheckpoint)

static uint32_t compute_checksum(const DeviceConfig &cfg) {
    uint32_t sum = 0x5A5A;
    const uint8_t *p = (const uint8_t *)&cfg;
    for (size_t i = 0; i < offsetof(DeviceConfig, checksum); i++) {
        sum = (sum << 1) ^ p[i];
    }
    return sum;
}

void nvs_store_init() {
    EEPROM.begin(EEPROM_SIZE);
}

void nvs_store_load(DeviceConfig &cfg) {
    EEPROM.get(EEPROM_CFG_OFFSET, cfg);
    if (cfg.checksum != compute_checksum(cfg)) {
        // Invalid — zero out
        memset(&cfg, 0, sizeof(cfg));
        strncpy(cfg.hostname, "cpapdash", sizeof(cfg.hostname));
        strncpy(cfg.api_url, DEFAULT_API_URL, sizeof(cfg.api_url));
    }
}

void nvs_store_save(const DeviceConfig &cfg) {
    DeviceConfig copy = cfg;
    copy.checksum = compute_checksum(copy);
    EEPROM.put(EEPROM_CFG_OFFSET, copy);
    EEPROM.commit();
}

uint32_t nvs_store_get_str_size() {
    uint32_t sz;
    EEPROM.get(STR_SIZE_OFFSET, sz);
    if (sz == 0xFFFFFFFF) return 0;  // Uninitialized
    return sz;
}

void nvs_store_set_str_size(uint32_t size) {
    EEPROM.put(STR_SIZE_OFFSET, size);
    EEPROM.commit();
}

uint32_t nvs_store_get_checkpoint(const char *date_folder, const char *filename) {
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        FileCheckpoint ck;
        EEPROM.get(CKPT_OFFSET + i * CKPT_SIZE, ck);
        if (ck.date_folder[0] == 0 || ck.date_folder[0] == (char)0xFF) continue;
        if (strcmp(ck.date_folder, date_folder) == 0 &&
            strcmp(ck.filename, filename) == 0) {
            return ck.offset;
        }
    }
    return 0;
}

void nvs_store_set_checkpoint(const char *date_folder, const char *filename, uint32_t offset) {
    // Find existing or oldest slot
    int slot = -1;
    uint32_t min_offset = UINT32_MAX;
    int min_slot = 0;

    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        FileCheckpoint ck;
        EEPROM.get(CKPT_OFFSET + i * CKPT_SIZE, ck);

        // Exact match — update in place
        if (ck.date_folder[0] != 0 && ck.date_folder[0] != (char)0xFF &&
            strcmp(ck.date_folder, date_folder) == 0 &&
            strcmp(ck.filename, filename) == 0) {
            slot = i;
            break;
        }

        // Empty slot
        if (ck.date_folder[0] == 0 || ck.date_folder[0] == (char)0xFF) {
            if (slot == -1) slot = i;
            continue;
        }

        // Track oldest (smallest offset) for eviction
        if (ck.offset < min_offset) {
            min_offset = ck.offset;
            min_slot = i;
        }
    }

    if (slot == -1) slot = min_slot;  // Evict oldest

    FileCheckpoint ck;
    memset(&ck, 0, sizeof(ck));
    strncpy(ck.date_folder, date_folder, sizeof(ck.date_folder) - 1);
    strncpy(ck.filename, filename, sizeof(ck.filename) - 1);
    ck.offset = offset;

    EEPROM.put(CKPT_OFFSET + slot * CKPT_SIZE, ck);
    EEPROM.commit();
}

uint8_t nvs_store_get_mode() {
    uint8_t mode = EEPROM.read(MODE_OFFSET);
    if (mode > 1) return OP_MODE_FILE_SERVER;  // Default
    return mode;
}

void nvs_store_set_mode(uint8_t mode) {
    EEPROM.write(MODE_OFFSET, mode);
    EEPROM.commit();
}

void nvs_store_clear_wifi() {
    // WiFiManager handles its own credential storage
    // This just clears EEPROM checkpoints
    for (int i = CKPT_OFFSET; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.put(STR_SIZE_OFFSET, (uint32_t)0);
    EEPROM.commit();
}
