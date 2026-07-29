# Direct LED Output

## Backend

The ESP32-S3 drives seven WS2812 lanes through FastLED's
`LCD_CLOCKLESS` channel API. The active FastLED revision is pinned to:

```text
fa79f3f757ca2dadd5db7773b2bed5c13b26b33a
```

The implementation explicitly enables only `Bus::LCD_CLOCKLESS` and assigns
that bus to every channel. It does not use the older default `addLeds` routing.
The active backend is `firmware/esp32_controller/led_direct_output.*`.

## Lane Map

| Lane | Zone | GPIO | Pixels | Frame start |
|---:|---|---:|---:|---:|
| 1 | Z1 Mouth | 1 | 208 | 0 |
| 2 | Z2 Shoulder | 2 | 325 | 208 |
| 3 | Z3 Midbody | 39 | 400 | 533 |
| 4 | Z4 Rear | 40 | 300 | 933 |
| 5 | Z5 Front legs | 41 | 300 | 1233 |
| 6 | Z6 Back legs | 42 | 300 | 1533 |
| 7 | Z7 Digestive | 43 | 75 | 1833 |

The animation engine and physical FastLED frame both contain 1,908 contiguous
`CRGB` pixels. Z3 is registered and animated as a full 400-pixel lane. The
pinned driver therefore exercises its multi-chunk transmission path for Z3.
Channels use GRB wire order.

GPIO0 is required internally by the ESP32-S3 LCD/I80 peripheral for
dummy/padding signals. It has no external connection and carries no animation
lane.

## Electrical Path

```text
ESP32-S3 GPIO
-> SN74AHCT244 3.3 V-to-5 V logic buffer
-> 100 ohm series resistor
-> zone DIN
```

Use regulated 5 V for the buffer. ESP32, buffer, and LED-system grounds must be
common. LED power injection, fusing, and heavy-current wiring remain separate.

## Startup and Diagnostics

Channel registration occurs during setup. The first frame is sent
automatically in `ANIMATION` mode.

Startup hardware check:

- starts after Serial, settings, animation, and FastLED initialization;
- runs before Wi-Fi/web initialization;
- continuously renders and transmits moving animation for five seconds;
- uses temporary 4–15% brightness and fixed 100% speed;
- bypasses saved master, ambient/active, global-look, zone, and speed values;
- never modifies or saves settings;
- transmits normal saved rendering immediately when finished.

First-show Serial output records:

- each enqueued channel and selected driver;
- internal, DMA, and PSRAM free/largest heap blocks before and after show;
- whether all seven channels were routed to `LCD_CLOCKLESS`.

`ROUTING CONFIRMED` confirms channel routing only. `firstShowAttempted=1`
confirms that `FastLED.show()` was called. FastLED ESP32 error logging and full
error handling are enabled during hardware validation so allocation/peripheral
failures remain visible in Serial. Neither status proves electrical output;
signal integrity, logic shifting, grounding, power, and light remain physical
checks.

## Retired Output Expander

Pixelblaze Output Expander UART output is not part of the active firmware.
GPIO39 belongs to Z3. The vendored PBDriverAdapter remains only as historical
source and must not be re-enabled without a new hardware/pin review. No
Output Expander simulator or compatibility API remains in the active backend.
