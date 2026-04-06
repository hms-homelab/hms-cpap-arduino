#pragma once
#include <ESP8266WebServer.h>

// ezShare-compatible HTTP file server (AP mode)
// Port of hms-cpap-fysetc/main/file_server.c

void file_server_init(ESP8266WebServer &server);
