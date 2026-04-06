#pragma once
#include "file_scanner.h"

// Cloud push client (STA mode)
// Port of hms-cpap-fysetc/main/http_pusher.c

void http_pusher_init(const char *api_url, const char *serial, const char *secret);
bool http_pusher_upload_edf(const pending_file_t *file);
bool http_pusher_upload_str(uint32_t offset, uint32_t size);
bool http_pusher_heartbeat();
bool http_pusher_sync(scan_result_t *scan);
