# Éclair 7 ordinary FastLED/RMT proof

> Standalone hardware proof only; this is not the production Éclair firmware.
> Production is `firmware/eclair_controller`. This proof uses the same verified
> RMT4 toolchain so all seven physical lanes can be tested independently.

Standalone physical proof that one Éclair ESP32-S3 can drive all seven Tardi LED zones with ordinary FastLED `addLeds()` controllers.

## Required configuration

- Board: ESP32-S3-DevKitC-1 / ESP32-S3-WROOM-1-N8R8
- Flash/PSRAM: 8 MB / 8 MB OPI
- Arduino-ESP32: 2.0.17 / ESP-IDF 4.4.7
- FastLED: 3.9.20
- USB Mode: Hardware CDC and JTAG
- USB CDC On Boot: Enabled
- LED type/order: WS2812B / GRB
- Common ESP32 and LED-power ground
- Direct data path: ESP32-S3 GPIO to LED DIN

IDF4 selects the RMT4 scheduler without a `FASTLED_RMT5` override. The adjacent
`build_opt.h` supplies the required global options:

```text
-DESP32_ARDUINO_NO_RGB_BUILTIN=1
-DFASTLED_RMT_MAX_CHANNELS=4
-DFASTLED_RMT_MEM_BLOCKS=1
```

Do not replace that compiler option with a C++ `#define` inside the sketch.

## Compile-check status

Arduino-ESP32 2.0.17 with FastLED 3.9.20 compiles all seven controllers. The
verified ELF contains `ESP32RMTController`, `startNext()` and `RMTMEM`, with no
`ClocklessBlockingGeneric` fallback.

The rejected Arduino-ESP32 3.3.10/FastLED 3.10.4 pairing failed in the forced
RMT4 implementation. Do not restore it merely because an alternate fallback
can compile. Binary driver checks still do not prove physical LED output.

Verified clean proof build: 321,033 bytes flash and 26,108 bytes static RAM.

## Lane map

| Zone | GPIO | Pixels |
|---|---:|---:|
| Z1 | 4 | 208 |
| Z2 | 5 | 325 |
| Z3 | 6 | 400 |
| Z4 | 7 | 300 |
| Z5 | 8 | 300 |
| Z6 | 9 | 300 |
| Z7 | 10 | 75 |

Total: 1,908 pixels.

## Flash and monitor

1. Install the exact Arduino-ESP32 and FastLED versions above.
2. Open `eclair7_rmt_proof.ino` from this sketch folder.
3. Select the board, 8 MB flash, OPI PSRAM, Hardware CDC/JTAG and USB CDC On Boot settings above.
4. Connect the seven data lanes and common ground, then compile and upload.
5. Open the native USB Serial Monitor at 115200 baud.

## Physical test

On boot, the sketch registers seven software controllers and deliberately sends a black frame first. It then cycles visible solid, moving-marker, alternating, rainbow and RGB-band patterns at approximately 30 complete updates per second.

Serial reports controller registration, whether the first `FastLED.show()` returned, rolling show-time minimum/average/maximum, stalls, heap and any FastLED/RMT diagnostics emitted by the library. Registration and a returned `show()` do not prove electrical transmission or visible light.

Manually confirm:

- all seven physical lanes light and animate;
- each lane reaches its full pixel count, especially all 400 Z3 pixels;
- GRB colour order is correct;
- patterns remain stable over time;
- no lane flickers, truncates or cross-controls another lane;
- native USB upload and Serial diagnostics remain reliable;
- Serial shows no RMT allocation, timeout or transmission errors.
