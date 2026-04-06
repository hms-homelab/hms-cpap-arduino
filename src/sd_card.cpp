#include "sd_card.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

static SdFat sd;
static bool s_mounted = false;

void sd_card_init() {
    // CS_SENSE not used for now — CPAP bus detection needs
    // a different approach (timer/polling based)
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
}

bool sd_card_take_bus() {
    return true;
}

void sd_card_release_bus() {
    digitalWrite(SD_CS_PIN, HIGH);
}

bool sd_card_bus_available() {
    return true;
}

bool sd_card_mount() {
    if (s_mounted) return true;

    // SHARED_SPI: SdFat uses SPI.beginTransaction/endTransaction per operation
    // This allows sd.begin() to succeed on repeated calls
    if (!sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SPI_SPEED))) {
        Serial.println(F("[sd] Mount failed"));
        return false;
    }

    s_mounted = true;
    return true;
}

void sd_card_unmount() {
    if (!s_mounted) return;
    s_mounted = false;
    // Deselect and end SPI so CPAP can use the bus
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.end();
}

bool sd_card_is_mounted() {
    return s_mounted;
}

SdFat &sd_card_get() {
    return sd;
}
