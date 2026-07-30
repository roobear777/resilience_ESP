# Éclair 7 ordinary FastLED/RMT proof

> Historical diagnostic only; this is not the production Éclair firmware.
> Production is `firmware/eclair_controller`. This sketch intentionally retains
> the failed Arduino-ESP32 3.3.10 / forced-RMT4 reproduction. The production
> resolution is Arduino-ESP32 2.0.17 / IDF 4.4.7 with the pinned FastLED commit.

Standalone physical proof that one Éclair ESP32-S3 can drive all seven Tardi LED zones with ordinary FastLED `addLeds()` controllers.

## Required configuration

- Board: ESP32-S3-DevKitC-1 / ESP32-S3-WROOM-1-N8R8
- Flash/PSRAM: 8 MB / 8 MB OPI
- Arduino-ESP32: 3.3.10
- FastLED: 3.10.4
- USB Mode: Hardware CDC and JTAG
- USB CDC On Boot: Enabled
- LED type/order: WS2812B / GRB
- Common ESP32 and LED-power ground
- Direct data path: ESP32-S3 GPIO to LED DIN

The sketch forces the legacy RMT4 scheduler with `FASTLED_RMT5=0` before including FastLED. The adjacent `build_opt.h` supplies the required global compiler option:

```text
-DESP32_ARDUINO_NO_RGB_BUILTIN=1
```

Do not replace that compiler option with a C++ `#define` inside the sketch.

## Current compile-check status

The exact Arduino-ESP32 3.3.10 / FastLED 3.10.4 combination does not currently compile with forced RMT4. With the required files exactly as above, the sketch selects RMT4 but FastLED's separately compiled implementation selects its IDF5 default, ending in an undefined `fl::ChannelEngineRMT4::create()` linker symbol. Supplying `FASTLED_RMT5=0` globally as a diagnostic exposes further compile errors inside FastLED 3.10.4's RMT4 implementation for ESP32-S3/IDF5, including unavailable `RMTMEM` and `ChannelEngineRMT4Impl` interface mismatches.

This is a pinned-library/toolchain blocker, not evidence about physical LED output. Do not treat the flashing steps below as completed until that compile incompatibility is resolved or a different approved toolchain is selected.

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
