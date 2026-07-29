# Current Baseline

This is the authoritative behavior summary for the current live firmware.

## Controller

```text
Board: ESP32-S3-DevKitC-1-N8R8
Arduino core: 3.3.10
USB CDC On Boot: Enabled
Serial: native USB, 115200 baud
```

The ESP32 owns button debounce, accepted interactions, FIRE outputs and
cutoffs, LED rendering, direct LED transmission, saved LED settings, Serial
diagnostics, and the Wi-Fi controller.

## Live Configuration

```cpp
ENABLE_REAL_FASTLED_OUTPUT = true
FIRE_OUTPUTS_ENABLED = true
USE_INTERNAL_PULLDOWNS = false
```

OLED support and Output Expander output are absent from the active firmware.
No command or setup button is required for normal operation.

## Boot

1. FIRE pins are driven HIGH/idle.
2. Saved LED settings are loaded, or full defaults are used.
3. Seven `LCD_CLOCKLESS` lanes are registered.
4. A five-second moving LED hardware check runs at temporary 4–15% brightness
   and fixed 100% speed, independent of saved brightness and speed settings.
5. Normal rendering resumes immediately from the untouched saved settings.
6. The Wi-Fi AP starts and normal loop operation begins.

First-show diagnostics report channel routing and heap state. Physical LED
behavior must still be confirmed on hardware.

## Inputs and FIRE

Buttons are active-HIGH and use external 10k pull-downs.

| Button | GPIO | Normal FIRE | LED |
|---:|---:|---:|---|
| 1 | 4 | FIRE1 | Z1 Mouth |
| 2 | 5 | FIRE2 | Z2 Shoulder |
| 3 | 6 | FIRE3 | Z3 Midbody |
| 4 | 7 | FIRE4 | Z4 Rear |
| 5 | 15 | FIRE5 | Z5 Front legs |
| 6 | 16 | FIRE6 | Z6 Back legs |
| 7 | 17 | FIRE7 | Z7 Digestive |
| 8 | 18 | FIRE8 | none |

Normal FIRE1–FIRE8 behavior is a 100 ms pulse on press, repeated every
1,000 ms while held. FIRE outputs are active-LOW.

Button 1 + Button 8 requests FIRE9/Head Poof while held, with a 10-second
cutoff, and activates all seven LED zones.

Holding all eight buttons has priority: FIRE1–FIRE9 pulse together for 500 ms
once, normal repeats and Head Poof are suppressed, and release of at least one
button is required to re-arm.

## LEDs

The direct output map is defined in `docs/direct_led_output.md`. The animation
engine and FastLED frame contain 1,908 pixels. Z3 is a full 400-pixel logical
and physical animation lane. Ambient rendering is continuous. Accepted button
events activate zones for the saved animation duration, which defaults to 10
seconds.

LED output mode and validation modes are runtime-only. Reboot always returns
to automatic animation. Saved appearance settings persist across power cycles
only after `SAVE`.

## Web

The `TARDI-LED` AP and captive portal are available while powered at
`http://192.168.4.1`. The page controls LEDs only and cannot activate FIRE
outputs. It is served as one static gzip asset and loads live values through
`/api/status`.
