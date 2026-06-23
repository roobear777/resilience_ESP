# LED Output Expander Notes

This document is the reference/README for the ESP32-S3 to Pixelblaze Output Expander LED output path. Animation structure belongs in `docs/led_animation_architecture.md`.

## Hardware Reference

Confirmed target hardware:

```text
ElectroMage Pixelblaze Output Expander v3.0
electromage.com Output Expander v3.0 @2021 Ben Hencke
```

Product reference:

```text
https://shop.electromage.com/products/pixelblaze-output-expander-serial-to-8x-ws2812-apa102-driver
```

Use that current ElectroMage product page as the capacity reference for this project. Older Output Expander README/version information can mention lower limits; do not use those older limits unless they clearly apply to a different board version.

Relevant capacity:

```text
up to 800 RGB pixels per channel for WS2812 / NeoPixel-style output
```

The largest current Tardi channel is Ch3 / Midbody at 400 pixels, so the current channel lengths are within the documented capacity.

## Wiring

```text
ESP32 GPIO39 TX -> Output Expander DAT / UART input
ESP32 GND       -> Output Expander GND
UART baud       = 2000000
```

The UART link is TX-only from ESP32 to Output Expander.

`CLK` is unused for WS2811/WS2812-style LEDs. It only matters for APA102/DotStar-style LEDs.

Shared ground is required. Do not connect ESP32 GPIO pins to 5V logic.

GPIO39 is the planned UART TX pin. It is JTAG-related, so reset/default-state behaviour is a hardware check. Firmware holds GPIO39 at UART-idle HIGH after setup begins, but firmware cannot control the pre-firmware reset/bootloader window.

Fallbacks if GPIO39 physically fails:

```text
GPIO40 first
GPIO41 second
GPIO38 only after a fresh pin review if the issue appears JTAG-related
```

Do not use GPIO14, GPIO16, or GPIO17 as fallbacks because they are already used. Avoid GPIO35/36/37 on the N8R8/octal PSRAM path. Avoid GPIO19/20 because of USB-related caveats.

## PBDriverAdapter

`PBDriverAdapter` is vendored locally:

```text
firmware/esp32_controller/src/PBDriverAdapter/
```

Local patch:

```cpp
void PBDriverAdapter::begin(uint32_t uartFrequency, int8_t txPin);
```

This lets the firmware use GPIO39 instead of the upstream ESP32 default TX pin.

Do not modify vendored PBDriverAdapter files unless a task explicitly asks for driver work.

## Channel Mapping

| Channel | Physical LEDs | Firmware zone | Pixels | Logical start |
|---:|---|---|---:|---:|
| Ch0 | Button station LEDs | Z8 | 100 | 1908 |
| Ch1 | Mouth LEDs | Z1 | 208 | 0 |
| Ch2 | Shoulder LEDs | Z2 | 325 | 208 |
| Ch3 | Midbody LEDs | Z3 | 400 | 533 |
| Ch4 | Rear LEDs | Z4 | 300 | 933 |
| Ch5 | Front leg LEDs | Z5 | 300 | 1233 |
| Ch6 | Back leg LEDs | Z6 | 300 | 1533 |
| Ch7 | Digestive LEDs | Z7 | 75 | 1833 |

Total logical pixels:

```text
2008
```

Mouth / Zone 1 is on Ch1, not Ch0. Ch0 is the button-station LED output.

## Colour Order

The PBChannel metadata currently uses RGB indexes:

```text
redi   = 0
greeni = 1
bluei  = 2
```

Do not change animation HSV/RGB conversion when adjusting Output Expander channel metadata.

## Power Notes

The Output Expander is LED data output hardware, not the whole LED power system.

LED power injection, current capacity, fusing, and ground distribution remain separate sculpture wiring concerns.

The standard v3.0 board is not the Pro fused power-distribution board. Pro hardware is only relevant if the installation needs Pro-specific power-distribution features.
