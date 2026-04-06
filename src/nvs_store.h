#pragma once
#include <stdint.h>

// EEPROM-based persistent storage (replaces ESP-IDF NVS)
// Stores: device config + file upload checkpoints

struct DeviceConfig {
    char hostname[32];
    uint8_t ntp_source;       // 0=none, 1=gateway, 2=server
    char ntp_server[64];
    char device_serial[24];
    char api_key[48];
    char api_url[96];
    uint32_t checksum;
};

struct FileCheckpoint {
    char date_folder[9];      // "20260401\0"
    char filename[33];        // "20260401_233000_BRP.edf\0"
    uint32_t offset;
};

void nvs_store_init();
void nvs_store_load(DeviceConfig &cfg);
void nvs_store_save(const DeviceConfig &cfg);

uint32_t nvs_store_get_checkpoint(const char *date_folder, const char *filename);
void nvs_store_set_checkpoint(const char *date_folder, const char *filename, uint32_t offset);

uint32_t nvs_store_get_str_size();
void nvs_store_set_str_size(uint32_t size);

void nvs_store_clear_wifi();

uint8_t nvs_store_get_mode();      // OP_MODE_FILE_SERVER or OP_MODE_CLOUD_PUSH
void nvs_store_set_mode(uint8_t mode);
