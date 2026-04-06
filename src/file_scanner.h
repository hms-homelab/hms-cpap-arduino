#pragma once
#include "config.h"
#include <stdint.h>

struct pending_file_t {
    char date_folder[9];
    char filename[33];
    uint32_t offset;
    uint32_t size;
};

struct scan_result_t {
    pending_file_t files[MAX_PENDING_FILES];
    int count;
    bool str_changed;
    uint32_t str_offset;
    uint32_t str_size;
};

// Scan DATALOG/ for EDF files with new data (port of hms-cpap-fysetc file_scanner.c)
scan_result_t file_scanner_scan();
