# Eclair7 production firmware

Eclair7 is a two-board build. Tardi keeps all buttons, FIRE outputs, web UI,
saved settings, accepted triggers, diagnostic modes, Head Poof, and safety
cutoffs. It sends complete CRC-checked LED-state snapshots over UART1. Eclair
renders the shared production animation engine and drives Z1-Z7. A 500 ms link
timeout forces Eclair black until valid state returns.

## Hardware and wiring

Both boards are ESP32-S3-DevKitC-1 with ESP32-S3-WROOM-1-N8R8 modules: 8 MB
flash and 8 MB octal PSRAM. Native USB stays on GPIO19/GPIO20.

```text
Tardi GPIO40 TX -> Eclair GPIO18 RX
Tardi GPIO41 RX <- Eclair GPIO17 TX
Tardi GND       --- Eclair GND --- LED-power GND

Eclair GPIO4  -> Z1 DIN (208)    Eclair GPIO8  -> Z5 DIN (300)
Eclair GPIO5  -> Z2 DIN (325)    Eclair GPIO9  -> Z6 DIN (300)
Eclair GPIO6  -> Z3 DIN (400)    Eclair GPIO10 -> Z7 DIN (75)
Eclair GPIO7  -> Z4 DIN (300)
```

Total: 1,908 WS2812-class pixels, GRB order. The selected electrical assumption
is direct 3.3 V ESP32-S3 GPIO to 5 V LED DIN, with common ground and no buffer,
level shifter, or series data resistor. This electrical path is not yet proven
on the finished installation.

## Exact toolchain assumptions

The two boards intentionally use different Arduino-ESP32 cores:

| Target | Arduino-ESP32 | ESP-IDF family | FastLED |
|---|---:|---:|---:|
| Tardi | 3.3.10 | 5.x | not linked |
| Eclair | 2.0.17 | 4.4.7 | 3.9.20 |

PlatformIO `espressif32@6.10.0` resolves to Arduino-ESP32 2.0.17 / IDF 4.4.7
and the local `platformio.ini` pins both that platform and FastLED 3.9.20.

Why the older Eclair core is deliberate:

- Seven outputs require FastLED's RMT4 worker pool on the ESP32-S3's four TX
  channels; Eclair's `build_opt.h` globally sets four channels and one RMT
  memory block per active channel.
- Arduino-ESP32 3.3.10 with FastLED 3.10.4 cannot build the forced RMT4 path:
  the sketch-local selection produces a missing RMT4 factory symbol, while a
  global selection exposes IDF5 interface and `RMTMEM` errors inside FastLED.
- Arduino-ESP32 2.0.17 uses IDF 4.4.7, selects FastLED 3.9.20 RMT4 normally,
  and compiles all seven registered controllers. Do not add `FASTLED_RMT5=0`
  to this build.

The compiled Eclair ELF must contain `ESP32RMTController`, `startNext()` and
`RMTMEM`, and must not contain `ClocklessBlockingGeneric`. This confirms the
selected software driver, not physical light output.

Arduino IDE/CLI settings for both targets are ESP32S3 Dev Module, 8 MB flash,
OPI PSRAM, Hardware CDC, and USB CDC On Boot enabled. Tardi additionally uses
the 8 MB default partition scheme. Boards Manager normally keeps one version
of an ESP32 core at a time, so either switch versions between builds or use a
separate Arduino CLI data/config directory for Eclair's 2.0.17 installation.
The old minimal Tardi `platformio.ini` is not its verified 3.3.10 build path.

Verified clean builds:

```text
Tardi  3.3.10:                    950,314 bytes flash; 47,168 bytes static RAM
Eclair 2.0.17 / FastLED 3.9.20:  335,385 bytes flash; 26,468 bytes static RAM
```

Physical validation must still confirm all seven lanes and the complete
400-pixel Z3 multi-chunk lane under real power and wiring.

## Shared-source synchronization

Arduino does not compile sibling-sketch sources, so Eclair contains exact local
copies of the canonical LED engine and link protocol. After changing either in
`../esp32_controller`, run `sh sync_shared_led_sources.sh`, verify no mismatch,
and compile both targets. Do not edit `src/shared` independently.
