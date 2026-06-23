# Pin Mapping

This file maps logical controller functions to ESP32-S3 GPIO pins.

All firmware logic should use the centralized constants/arrays in `firmware/esp32_controller/esp32_controller.ino` instead of scattered raw pin numbers.

## Board

```text
Board:  ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module
USB Serial/debug baud: 115200
```

Normal Arduino IDE upload uses the Upload button. Use BOOT/RESET only as a recovery step if upload stalls at `Connecting...`.

## OLED

| Function | GPIO |
|---|---:|
| OLED SDA | GPIO1 |
| OLED SCL | GPIO2 |

OLED module:

```text
SSD1306 128x64 I2C
likely address 0x3C
VCC = 3.3V
```

Libraries:

```text
Adafruit SSD1306
Adafruit GFX Library
```

## Buttons

Buttons are active-HIGH with external pull-down resistors.

```text
released = LOW through external 10k pull-down
pressed  = HIGH / 3.3V
```

| Button | GPIO |
|---:|---:|
| Button 1 | GPIO4 |
| Button 2 | GPIO5 |
| Button 3 | GPIO6 |
| Button 4 | GPIO7 |
| Button 5 | GPIO15 |
| Button 6 | GPIO16 |
| Button 7 | GPIO17 |
| Button 8 | GPIO18 |

Do not connect button inputs to 5V logic.

## FIRE Outputs

FIRE outputs are active-LOW.

```text
HIGH = idle / relay inactive
LOW  = trigger / relay active
```

| FIRE output | GPIO |
|---:|---:|
| FIRE1 | GPIO8 |
| FIRE2 | GPIO9 |
| FIRE3 | GPIO10 |
| FIRE4 | GPIO11 |
| FIRE5 | GPIO12 |
| FIRE6 | GPIO13 |
| FIRE7 | GPIO14 |
| FIRE8 | GPIO21 |
| FIRE9 / Big Poof | GPIO47 |

Normal FIRE1-FIRE8 pulse:

```text
500 ms
```

Big Poof FIRE cutoff:

```text
10 seconds maximum
```

## Pixelblaze Output Expander

```text
ESP32 GPIO39 TX -> Output Expander DAT / UART input
ESP32 GND       -> Output Expander GND
baud            = 2000000
```

Do not use board-labelled TX/RX / UART0 for the Output Expander.

Do not use these pins for Output Expander TX:

- GPIO16: Button 6
- GPIO23: old PBDriverAdapter upstream default, not Tardi wiring
- GPIO14: FIRE7
- GPIO17: Button 7
- GPIO19/GPIO20: USB-related caveats
- GPIO35/GPIO36/GPIO37: avoid on N8R8/octal PSRAM path

Fallbacks only if GPIO39 physically fails:

```text
GPIO40 first
GPIO41 second
GPIO38 only after a fresh pin review
```

## Reserved / Caution Pins

| Pin | Rule |
|---:|---|
| GPIO0 | avoid; boot/download related |
| GPIO45 | avoid unless deliberately reviewed; strap pin |
| GPIO46 | avoid unless deliberately reviewed; strap pin |
| GPIO48 | avoid; onboard RGB/status LED related |
| GPIO1/GPIO2 | reserved for OLED |
| board TX/RX | reserved for USB/programming/debug |

## Related Docs

```text
docs/gpio_schema.md
docs/current_baseline.md
docs/interaction_logic.md
docs/led_output_expander.md
```
