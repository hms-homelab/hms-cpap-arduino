#pragma once
#include <SdFat.h>

// SPI SD card management (from Steigan sdControl)
// Handles SPI bus arbitration with external master (CPAP)

void sd_card_init();
bool sd_card_mount();
void sd_card_unmount();
bool sd_card_is_mounted();
SdFat &sd_card_get();

// Bus control (for boards shared with a printer/CPAP)
bool sd_card_take_bus();
void sd_card_release_bus();
bool sd_card_bus_available();
