# ESP32 LED Port Status

This is the main current handoff for the ESP32 LED port and Pixelblaze Output Expander integration.

## Current LED Port Summary

- ESP32 firmware now has a complete software-side render path for the Pixelblaze Output Expander.
- India-side runtime can simulate a full Output Expander frame without Output Expander hardware.
- Real UART output exists only behind a disabled-by-default guard.
- Physical Pixelblaze Output Expander and LED validation has not yet been done.

## Current Architecture

Current software pipeline:

```text
Button / LED state
-> ledEngineRenderPixel(logicalPixelIndex, nowMs)
-> LedColor HSV
-> ledColorToRgb()
-> LedRgbColor RGB
-> GRB packed callback bytes
-> PBDriverAdapter render callback
-> future PBDriverAdapter::show()
-> Pixelblaze Output Expander
```

`PBDriverAdapter` does not store pixel buffers. Its `show()` path is callback-based: it asks firmware for a logical pixel index and writes the returned bytes to the Output Expander frame.

The simulator exercises the same packed-pixel path without calling `PBDriverAdapter::show()`.

## Main Files

- `firmware/esp32_controller/led_color.h`: internal HSV `LedColor`.
- `firmware/esp32_controller/led_color_convert.h/.cpp`: HSV `LedColor` to normal RGB `LedRgbColor`.
- `firmware/esp32_controller/led_engine.h/.cpp`: logical pixel render entry point and zone dispatch.
- `firmware/esp32_controller/led_expander_output.h/.cpp`: Output Expander mapping, packed GRB helpers, simulator, diagnostics, and guarded real output path.
- `firmware/esp32_controller/src/PBDriverAdapter/src/PBDriverAdapter.hpp/.cpp`: vendored PBDriverAdapter serial protocol implementation.
- `firmware/esp32_controller/esp32_controller.ino`: controller coordinator, button/FIRE/OLED logic, LED trigger hooks, and simulator diagnostic call.

## PBDriverAdapter Local Patch

`PBDriverAdapter` is vendored locally under:

```text
firmware/esp32_controller/src/PBDriverAdapter/
```

A Tardi local patch added:

```cpp
void PBDriverAdapter::begin(uint32_t uartFrequency, int8_t txPin);
```

Reason: the upstream ESP32 path used a fixed TX pin, but Tardi needs GPIO39.

The existing `PBDriverAdapter::begin()` API was preserved. Future real output should use:

```text
baud = 2000000
tx   = GPIO39
```

Do not use board-labelled TX/RX / UART0 for the Output Expander.
Do not use GPIO16 because it is Button 6.
Do not use GPIO23 for Tardi real output.

## Real Output Guard

Real Output Expander output is disabled by default:

```cpp
ENABLE_REAL_PB_EXPANDER_OUTPUT = false
```

When false:

- `ledExpanderOutputBegin()` configures channels but does not start UART.
- `ledExpanderOutputUpdate()` returns before `PBDriverAdapter::show()`.
- `PBDriverAdapter::begin()` is not called.
- `PBDriverAdapter::show()` is not called.
- No Output Expander frames are sent.

When California later validates hardware, enabling this guard is the deliberate step that allows UART output.

## Output Expander Channel Mapping

Do not assume Zone 1 maps to Channel 0.

| Expander channel | Zone                        | Pixel count | Logical start index |
| ---------------: | --------------------------- | ----------: | ------------------: |
|              Ch0 | Z8 string/stations          |         100 |                1908 |
|              Ch1 | Z1 mouth                    |         208 |                   0 |
|              Ch2 | Z2 shoulder                 |         325 |                 208 |
|              Ch3 | Z3 midbody                  |         400 |                 533 |
|              Ch4 | Z4 rear                     |         300 |                 933 |
|              Ch5 | Z5 legs / first legs group  |         300 |                1233 |
|              Ch6 | Z6 legs / second legs group |         300 |                1533 |
|              Ch7 | Z7 digestive                |          75 |                1833 |

Total = 2008 pixels.

## Byte Order

Internal colour after conversion is normal RGB:

```cpp
LedRgbColor { r, g, b }
```

Output callback bytes are packed as GRB:

```text
byte 0 = green
byte 1 = red
byte 2 = blue
```

Current `PBChannel` colour order metadata:

```text
redi   = 1
greeni = 0
bluei  = 2
```

Final colour order still needs California bench validation with real red/green/blue LED output.

## Runtime Simulator Diagnostics

Expected Serial output shape:

```text
EXP REAL allowed=0 started=0 tx=39 baud=2000000
EXP SIM OK channels=8 pixels=2008 failed=0 checksum=<varies> byteOrder=GRB
CH0 start=1908 px=100 ...
CH1 start=0 px=208 ...
CH2 start=208 px=325 ...
CH3 start=533 px=400 ...
CH4 start=933 px=300 ...
CH5 start=1233 px=300 ...
CH6 start=1533 px=300 ...
CH7 start=1833 px=75 ...
```

The checksum changes with animation time/state and is not a fixed universal value.

Key pass values:

- `allowed=0`
- `started=0`
- `channels=8`
- `pixels=2008`
- `failed=0`
- `byteOrder=GRB`

## India-Side Validation Completed

- Arduino IDE compile passes.
- Runtime Serial simulator block observed.
- Idle buttons showed `B1..B8 = 0`.
- FIRE outputs shown idle in Serial.
- Simulator reports all 8 channels and all 2008 pixels.
- Real output remains disabled.

## Not Yet Validated

- No physical Pixelblaze Output Expander tested in India.
- No GPIO39 2 Mbps UART electrical validation yet.
- No real LED strip output yet.
- No colour order validation on actual LEDs yet.
- No real channel physical order validation yet.
- No sculpture power/injection validation yet.

## California Validation Checklist

Before enabling real output:

- Confirm board is ESP32-S3 DevKitC-1 / same pin mapping.
- Confirm firmware compiles/uploads.
- Confirm Serial shows:

```text
EXP REAL allowed=0 started=0 tx=39 baud=2000000
EXP SIM OK channels=8 pixels=2008 failed=0 byteOrder=GRB
```

- Wire ESP32 GPIO39 TX to Pixelblaze Output Expander DATA/input.
- Share ESP32 GND with Output Expander GND.
- Do not use board-labelled TX/RX / UART0.
- Do not use GPIO16.
- Confirm Output Expander power and LED power are correct before connecting LEDs.

When ready for a controlled test:

- Change `ENABLE_REAL_PB_EXPANDER_OUTPUT` to `true`.
- Upload.
- Confirm Serial status changes to `allowed=1` and `started=1` if diagnostics expose it.
- Start with a small safe LED/zone test if possible.
- Validate red/green/blue colour order.
- Validate Ch0..Ch7 physical zone order.
- Only then scale to full sculpture output.

## Safety Note

This firmware path controls LEDs only. FIRE/poofer logic is separate and must not be altered during Output Expander validation.

## Current Handoff State

- Software-side expander render path is implemented.
- Simulator passes at runtime.
- Real output code path compiles but is disabled.
- California hardware validation is the next required proof.
