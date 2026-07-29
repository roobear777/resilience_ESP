# ESP32 LED Port Status

## Implemented

- Seven direct `LCD_CLOCKLESS` lanes are compiled and enabled.
- Layout is Z1–Z7 with 1,908 logical and physical pixels.
- Z3 is a full 400-pixel animation lane, exercising FastLED multi-chunk output.
- Output starts automatically in animation mode.
- A five-second moving hardware check runs before Wi-Fi, then saved rendering resumes.
- Native USB CDC protects GPIO43 from UART0 ownership.
- OLED, Z8, setup-button GPIO40 ownership, and Output Expander UART output are removed from the active build.
- Final Arduino build fits the default application partition.

## Build Reference

```text
ESP32 Arduino core: 3.3.10
FastLED: fa79f3f757ca2dadd5db7773b2bed5c13b26b33a
USB CDC On Boot: Enabled
Last verified program size: 1,297,939 / 1,310,720 bytes
Static RAM: 54,760 / 327,680 bytes
```

The program has 12,781 bytes of flash headroom. The web controller is stored as
one deterministic gzip asset; PBDriverAdapter and OLED code are absent from the
linked firmware.

## Physical Validation Checklist

1. Confirm GPIO0 has no external connection.
2. Confirm GPIO19/GPIO20 are used for native USB.
3. Verify SN74AHCT244 power, enables, common ground, and 100 ohm outputs.
4. Boot and capture first-show Serial diagnostics.
5. Confirm `ROUTING CONFIRMED` reports seven `LCD_CLOCKLESS` lanes.
6. Confirm `firstShowAttempted=1` and check for FastLED allocation/peripheral errors.
7. Observe moving output on all seven lanes throughout the five-second startup check.
8. Test `led ch 1` through `led ch 7`.
9. Test fixed red, green, and blue.
10. Confirm all 400 Z3 pixels animate continuously across the chunk boundary.
11. Monitor heap and stability under Wi-Fi plus continuous animation.

Software channel routing does not prove physical output. DMA allocation,
signal integrity, power distribution, grounding, and real LED behavior remain
hardware checks.
