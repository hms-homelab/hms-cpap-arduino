# Hardware Reference: FYSETC SD-WiFi v2.1

## Board Identification

- **MCU:** ESP8285H16 (ESP8266 core + 2 MB embedded flash)
- **USB-to-Serial:** CH340E (USB VID:PID `1a86:7523`)
- **Form Factor:** Full-size SD card with micro-SD slot on board
- **Toggle Switch:** USB2UART (serial/flash) / Card Reader (mass storage)
- **Buttons:** FLASH (GPIO0), RST (reset)
- **LED:** GPIO2 (active low)
- **LDO:** 3.3V 600mA

## Chip Identification

The board ships with no markings identifying the MCU. To confirm the chip:

```bash
# Hold FLASH button, plug USB, release after 2s
esptool.py --port /dev/ttyUSB1 chip_id
# Output: "Chip is ESP8285H16"
```

The CH340E driver (`ch341` kernel module) must be loaded on Linux:

```bash
sudo modprobe ch341
```

## Pin Configuration

| GPIO | Function | Notes |
|------|----------|-------|
| 0 | FLASH/BOOT button | Hold during power-on for bootloader mode |
| 2 | LED | Active LOW |
| 4 | SD_CS | Chip select for micro-SD card |
| 5 | CS_SENSE | Monitors host device CS line (INPUT, interrupt) |
| 12 | MISO | SPI data out (shared with host) |
| 13 | MOSI | SPI data in (shared with host) |
| 14 | SCLK | SPI clock (shared with host) |

## Entering Bootloader Mode

The board requires a specific sequence to enter flash/download mode:

1. Unplug USB
2. Set toggle switch to USB2UART
3. Hold FLASH button
4. Plug USB in
5. Release FLASH after 2 seconds

The RST button alone does NOT enter bootloader — you must hold FLASH during power-on. There is no way to enter bootloader via software (DTR/RTS toggle does not work on this board).

## Power

When inserted in a host SD slot (CPAP, 3D printer), the board is powered from the host's 3.3V SD card supply. The ESP8285 + WiFi draws ~70-170mA depending on TX power. Some hosts may not supply enough current on the SD 3.3V rail.

When connected via USB, the CH340E provides 3.3V from USB through the onboard LDO (600mA max).

## SD Card

- Micro-SD slot on the underside of the PCB
- Supports FAT16 and FAT32 (up to 32 GB)
- No exFAT support (SdFat 2.2.2 limitation)
- SPI mode only (no SDIO — ESP8285 doesn't have SDIO peripheral)

## Physical Fit Issues

The CH340E USB connector and card reader circuitry sit near the center of the PCB. This prevents the card from inserting deep enough to latch in some SD slots. The CPAP (ResMed AirSense 10/11) SD slot requires full insertion depth to lock.

**Workaround:** Use an SD card extender ribbon cable. The board sits outside the CPAP with the ribbon running to the SD slot.

**Note:** The FYSETC SD WiFi Pro does NOT have this problem — its USB is on the edge of the PCB.
