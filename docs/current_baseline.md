# Current Baseline

This file records the current live sculpture baseline for the Tardi Controller.

Detailed references:

```text
docs/gpio_schema.md
docs/pin_mapping.md
docs/interaction_logic.md
docs/led_animation_architecture.md
docs/led_output_expander.md
docs/web_setup_interface.md
```

Historical working notes live under `docs/archive/`.

## Controller Target

```text
Board:  ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module
USB Serial/debug baud: 115200
```

The ESP32-S3 owns:

- button debounce and accepted button events
- FIRE1-FIRE9 active-LOW outputs
- Big Poof detection and cutoff
- OLED status display
- Wi-Fi web controller
- LED animation rendering
- Pixelblaze Output Expander UART output

The ESP32 does not power LED loads, solenoids, relays, or high-current sculpture hardware directly.

## Live Build State

Current live-build expectations:

```text
ENABLE_REAL_PB_EXPANDER_OUTPUT = true
FIRE_OUTPUTS_ENABLED = true
USE_INTERNAL_PULLDOWNS = false
```

That means:

- real Output Expander output is permitted
- real FIRE output pins are active
- the button inputs rely on external pull-down resistors
- the LED runtime starts in normal animation mode after boot
- ambient LED rendering runs continuously for inactive zones
- the Wi-Fi web controller is available while the ESP32 is powered

Normal upload through Arduino IDE should use the Upload button. BOOT/RESET button handling is only a recovery step if upload stalls at `Connecting...`.

## Button Inputs

There are 8 active-HIGH button inputs:

| Button | GPIO |
|---:|---:|
| 1 | GPIO4 |
| 2 | GPIO5 |
| 3 | GPIO6 |
| 4 | GPIO7 |
| 5 | GPIO15 |
| 6 | GPIO16 |
| 7 | GPIO17 |
| 8 | GPIO18 |

Button wiring:

```text
released = LOW through external 10k pull-down
pressed  = HIGH / 3.3V
```

Do not connect ESP32 GPIO inputs to 5V logic.

## FIRE Outputs

FIRE outputs are active-LOW:

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

Normal FIRE1-FIRE8 pulse duration:

```text
500 ms
```

Big Poof FIRE cutoff:

```text
10 seconds maximum while Button 1 + Button 8 are held
```

The LED animation duration is separate from FIRE timing.

## LED Behaviour

Normal live LED behaviour:

- ambient LEDs start automatically after boot
- Button 1-7 trigger their matching LED zones
- triggered zones animate for the configured global animation duration
- triggered zones return to ambient when that duration expires
- Button 8 alone triggers FIRE8 but does not directly trigger an independent LED zone
- Z8 is the button-station LED zone; it mirrors/summarizes Z1-Z7 activity and participates in all-zone Big Poof
- Button 1 + Button 8 triggers Big Poof and activates all LED zones together for the configured global animation duration

Default LED animation duration:

```text
10 seconds
```

The web controller can change and save this duration. It affects normal zone animations and the all-zone Big Poof LED animation. It does not change the 500 ms FIRE pulse or 10 second Big Poof FIRE cutoff.

## Output Expander

Physical LED output target:

```text
ESP32-S3 GPIO39 TX -> Pixelblaze Output Expander DAT -> LED zones
ESP32 GND          -> Output Expander GND
UART baud          = 2000000
```

The UART is one-way from ESP32 TX to the Output Expander DAT input. `CLK` is unused for WS2811/WS2812-style LEDs.

Current channel map:

| Channel | Physical LEDs | Pixels | Logical start |
|---:|---|---:|---:|
| Ch0 | Button station LEDs | 100 | 1908 |
| Ch1 | Mouth LEDs | 208 | 0 |
| Ch2 | Shoulder LEDs | 325 | 208 |
| Ch3 | Midbody LEDs | 400 | 533 |
| Ch4 | Rear LEDs | 300 | 933 |
| Ch5 | Front leg LEDs | 300 | 1233 |
| Ch6 | Back leg LEDs | 300 | 1533 |
| Ch7 | Digestive LEDs | 75 | 1833 |

Mouth / Zone 1 is on Ch1, not Ch0.

Current PBDriverAdapter channel colour metadata is RGB:

```text
redi   = 0
greeni = 1
bluei  = 2
```

## Web Controller

The ESP32 starts the `TARDI-LED` Wi-Fi AP/captive portal while powered.

```text
SSID:     TARDI-LED
Password: tardigrade
Address:  http://192.168.4.1
```

The web page controls LED look/feel settings only. It does not include FIRE, relay, or hardware test controls.

Settings include brightness, colour intensity/saturation, speed, palette, behaviour, ambient/animation target selection, zone selection, per-zone values, and animation duration. Live changes apply in RAM; `SAVE` persists them.
