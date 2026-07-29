# Tardi Controller — Build Handoff

ESP32-S3 firmware for seven direct WS2812 lanes using FastLED
`LCD_CLOCKLESS`. OLED, Z8 and Pixelblaze Output Expander output are not part of
the active build.

## Required Toolchain

```text
Board: ESP32-S3-DevKitC-1-N8R8
Arduino board: ESP32S3 Dev Module
ESP32 Arduino core: 3.3.10
USB Mode: Hardware CDC and JTAG
USB CDC On Boot: Enabled
Serial: 115200 baud
FastLED commit: fa79f3f757ca2dadd5db7773b2bed5c13b26b33a
```

FastLED is not bundled with this repository. Install the exact pinned commit:

```text
https://github.com/FastLED/FastLED/archive/fa79f3f757ca2dadd5db7773b2bed5c13b26b33a.zip
```

In Arduino IDE use **Sketch → Include Library → Add .ZIP Library**. Do not
substitute an arbitrary Library Manager release: this build needs the pinned
ESP32-S3 `LCD_CLOCKLESS` multi-chunk implementation. Remove library conflicts
or confirm the compile log selects the pinned copy.

Open `firmware/esp32_controller/esp32_controller.ino`, compile and upload
normally. The build intentionally fails if USB CDC On Boot is disabled because
UART0 conflicts with GPIO43/Z7.

## LED Output

| Zone | GPIO | Pixels | Frame start |
|---:|---:|---:|---:|
| Z1 Mouth | 1 | 208 | 0 |
| Z2 Shoulder | 2 | 325 | 208 |
| Z3 Midbody | 39 | 400 | 533 |
| Z4 Rear | 40 | 300 | 933 |
| Z5 Front legs | 41 | 300 | 1233 |
| Z6 Back legs | 42 | 300 | 1533 |
| Z7 Digestive | 43 | 75 | 1833 |

Total: 1,908 pixels, GRB order. Z3 uses all 400 pixels and exercises FastLED's
multi-chunk path.

- GPIO0: internal `LCD_CLOCKLESS` clock/DC dummy; leave unwired.
- GPIO19/GPIO20: native USB D−/D+.
- GPIO38/GPIO48: temporary onboard-RGB diagnostic candidates; leave unwired.
- LED data passes through the documented 5 V SN74AHCT244 and 100 Ω outputs.
- ESP32, buffer and LED-system grounds must be common.

## Buttons and FIRE

```text
Buttons: GPIO4, 5, 6, 7, 15, 16, 17, 18
FIRE:    GPIO8, 9, 10, 11, 12, 13, 14, 21, 47
```

Buttons are active-HIGH with external 10k pull-downs. FIRE outputs are
active-LOW: HIGH idle, LOW triggered.

- FIRE1–FIRE8: 100 ms on press, repeating every 1,000 ms while held.
- Button 1 + Button 8: FIRE9/Head Poof while held, maximum 10 seconds.
- All eight buttons: FIRE1–FIRE9 for 500 ms once; release to re-arm.

## Startup and Validation

Boot automatically runs a five-second moving LED hardware check before Wi-Fi,
then restores the saved look. Serial reports FastLED errors, heap state and
channel routing. `ROUTING CONFIRMED` and `firstShowAttempted=1` do not prove
electrical output.

On the first hardware run, confirm all seven lanes and specifically all 400 Z3
pixels animate continuously across the multi-chunk boundary.

Useful Serial commands:

```text
led status
led settings
led animation
led off
led solid
led red | led green | led blue
led ch 1..7
rgb test | rgb off
wifi status
```

## Web Controller

```text
SSID: TARDI-LED
Password: tardigrade
Address: http://192.168.4.1
```

The web page controls LEDs only. Changes remain in RAM until `SAVE`.

Authoritative detail:

- `docs/current_baseline.md`
- `docs/gpio_schema.md`
- `docs/direct_led_output.md`
- `docs/interaction_logic.md`
