# ESP32 LED Port Status

This is the current LED/output handoff for the live ESP32-S3 sculpture build.

## Summary

- The ESP32 firmware owns the LED animation engine for Z1-Z8.
- Real Pixelblaze Output Expander output is enabled for the live build.
- Ambient LED animation starts automatically after boot.
- Button-triggered LED zones return to ambient automatically after the configured animation duration.
- The Output Expander path uses the vendored `PBDriverAdapter`.
- The development simulator remains useful for software checks, but it is not the normal live operation path.

## Render Pipeline

```text
controller / LED state
-> ledEngineRenderPixel(logicalPixelIndex, nowMs)
-> LedColor HSV
-> ledColorToRgb()
-> LedRgbColor RGB
-> PBDriverAdapter callback bytes
-> PBDriverAdapter::show()
-> Pixelblaze Output Expander
-> LED channels
```

`PBDriverAdapter::show()` is callback-based; it does not require a permanent full-frame pixel buffer in firmware.

## Main Files

- `firmware/esp32_controller/led_color.h`: internal HSV colour type.
- `firmware/esp32_controller/led_color_convert.h/.cpp`: HSV to RGB conversion.
- `firmware/esp32_controller/led_config.h`: logical LED counts and default timing constants.
- `firmware/esp32_controller/led_layout.h/.cpp`: logical zone ranges and helpers.
- `firmware/esp32_controller/led_state.h/.cpp`: active LED trigger windows.
- `firmware/esp32_controller/led_settings.h/.cpp`: saved LED look/feel settings.
- `firmware/esp32_controller/led_engine.h/.cpp`: pixel render entry point and settings application.
- `firmware/esp32_controller/led_expander_output.h/.cpp`: Output Expander mapping, driver callbacks, simulator, runtime modes, and real output path.
- `firmware/esp32_controller/esp32_controller.ino`: controller coordinator, button/FIRE/OLED/Serial/Web integration.

## PBDriverAdapter

`PBDriverAdapter` is vendored locally under:

```text
firmware/esp32_controller/src/PBDriverAdapter/
```

The local Tardi patch adds a caller-selected ESP32 TX pin overload:

```cpp
void PBDriverAdapter::begin(uint32_t uartFrequency, int8_t txPin);
```

The live build uses:

```text
TX pin = GPIO39
baud   = 2000000
```

Do not use board-labelled TX/RX / UART0 for the Output Expander. Do not use GPIO16, because it is Button 6. Do not use PBDriverAdapter's old default ESP32 TX GPIO23 for Tardi output.

## Real Output State

Current live-build guard:

```cpp
ENABLE_REAL_PB_EXPANDER_OUTPUT = true
```

Startup behaviour:

- GPIO39 is set to UART-idle HIGH during firmware startup.
- PBDriverAdapter channel metadata is configured.
- The real UART/driver path is permitted.
- Normal animation mode starts automatically, so ambient LED data is sent after boot.

GPIO39 idle setup:

```cpp
digitalWrite(PB_EXPANDER_TX_PIN, HIGH);
pinMode(PB_EXPANDER_TX_PIN, OUTPUT);
```

That idle setup is not a frame and does not by itself animate LEDs.

## Output Expander Channel Mapping

Do not assume Zone 1 maps to Channel 0.

| Expander channel | Physical LEDs | Firmware zone | Pixel count | Logical start |
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

Z8 is the button-station LED zone. Button 8 alone does not trigger an independent LED zone. Z8 mirrors/summarizes Z1-Z7 station activity and is included when Big Poof activates all LED zones.

## Colour Metadata

The physical red/green swap found during LED testing has been corrected in the PBChannel metadata.

Current `PBChannel` colour metadata:

```text
redi   = 0
greeni = 1
bluei  = 2
```

Internal animation colour still uses HSV converted to normal RGB before output.

## LED Trigger Timing

Normal zone triggers:

```text
Button 1 -> Z1
Button 2 -> Z2
Button 3 -> Z3
Button 4 -> Z4
Button 5 -> Z5
Button 6 -> Z6
Button 7 -> Z7
```

Button 8:

```text
Button 8 -> FIRE8 only; no independent LED zone 8 trigger
```

Big Poof:

```text
Button 1 + Button 8 -> FIRE9 / Big Poof + all LED zones active
```

Both normal zone triggers and Big Poof all-zone LED animation use the saved global animation duration. Default:

```text
10 seconds
```

After the duration expires, zones render ambient again.

## Output Expander Hardware Reference

Confirmed target hardware:

```text
ElectroMage Pixelblaze Output Expander v3.0
electromage.com Output Expander v3.0 @2021 Ben Hencke
```

Use the current ElectroMage product page as the capacity reference:

```text
https://shop.electromage.com/products/pixelblaze-output-expander-serial-to-8x-ws2812-apa102-driver
```

Relevant capacity: up to 800 RGB pixels per channel for WS2812/NeoPixel-style output. The largest current Tardi channel is Ch3 at 400 pixels, so the planned channel lengths are within that documented capacity.

Older Output Expander notes with lower per-channel limits should not be used as the capacity reference unless they clearly apply to a different board version.

## Development Simulator

The software simulator can still exercise the full 8-channel, 2008-pixel render traversal without physical Output Expander hardware.

Healthy simulator diagnostics include:

```text
EXP SIM OK channels=8 pixels=2008 failed=0 ... byteOrder=...
```

The checksum varies with animation time and state.

## Still Physical-Hardware Dependent

These remain physical deployment checks, not things the ESP32 can prove by itself:

- GPIO39 2 Mbps signal integrity
- real Output Expander response
- physical Ch0-Ch7 wiring order
- actual LED power injection and grounding under load
- whether all connected LEDs are WS2811/WS2812-style rather than APA102/DotStar
- whether Z7 physical wiring matches the 75 logical-pixel assumption
