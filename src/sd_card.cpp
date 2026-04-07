#include "sd_card.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

// Bus sharing via CS_SENSE interrupt + tri-state pin control.
// First mount does full sd.begin() (slow, ~200ms).
// Subsequent mounts just re-take the SPI pins (fast, <1ms).

static SdFat sd;
static bool s_mounted = false;
static bool s_card_initialized = false;
static volatile bool s_we_have_bus = false;
static volatile unsigned long s_blockout_until = 0;

#define SPI_BLOCKOUT_MS  3000

static IRAM_ATTR void cs_sense_isr() {
    if (!s_we_have_bus) {
        s_blockout_until = millis() + SPI_BLOCKOUT_MS;
    }
}

void sd_card_init() {
    pinMode(MISO_PIN, INPUT);
    pinMode(MOSI_PIN, INPUT);
    pinMode(SCLK_PIN, INPUT);
    pinMode(SD_CS_PIN, INPUT);
    pinMode(CS_SENSE_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CS_SENSE_PIN), cs_sense_isr, FALLING);
    Serial.println(F("[sd] Init — pins tri-stated, CS_SENSE armed"));
}

static void take_bus() {
    s_we_have_bus = true;
    pinMode(MISO_PIN, SPECIAL);
    pinMode(MOSI_PIN, SPECIAL);
    pinMode(SCLK_PIN, SPECIAL);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
}

static void release_bus() {
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(MISO_PIN, INPUT);
    pinMode(MOSI_PIN, INPUT);
    pinMode(SCLK_PIN, INPUT);
    pinMode(SD_CS_PIN, INPUT);
    s_we_have_bus = false;
}

bool sd_card_take_bus() {
    if (millis() < s_blockout_until) return false;
    take_bus();
    return true;
}

void sd_card_release_bus() {
    release_bus();
}

bool sd_card_bus_available() {
    return millis() >= s_blockout_until;
}

bool sd_card_mount() {
    if (s_mounted) return true;

    if (!sd_card_bus_available()) return false;

    take_bus();

    if (!s_card_initialized) {
        // First mount: full init (~200ms)
        unsigned long t0 = millis();
        if (!sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SPI_SPEED))) {
            delay(100);
            if (!sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SPI_SPEED))) {
                Serial.println(F("[sd] Mount failed"));
                release_bus();
                return false;
            }
        }
        s_card_initialized = true;
        Serial.printf("[sd] First mount: %lums\n", millis() - t0);
    }
    // Subsequent mounts: pins already configured, SdFat state preserved
    // Just re-took the bus pins — card is ready

    s_mounted = true;
    return true;
}

void sd_card_unmount() {
    if (!s_mounted) return;
    s_mounted = false;
    // Do NOT call SPI.end() — keep SdFat's internal state
    // Just tri-state the pins so CPAP can use the bus
    release_bus();
}

bool sd_card_is_mounted() {
    return s_mounted;
}

SdFat &sd_card_get() {
    return sd;
}
