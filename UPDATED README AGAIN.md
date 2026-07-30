# Eclair7 production build

Eclair7 uses two ESP32-S3-DevKitC-1/WROOM-1-N8R8 boards. Tardi retains all
buttons, active-LOW FIRE outputs and cutoffs, accepted interactions, web UI,
Wi-Fi, Serial commands, and saved LED settings. Eclair receives complete
CRC-checked state snapshots, renders the shared production engine, and drives
all seven WS2812B/GRB zones.

## Production map

```text
Tardi GPIO40 TX -> Eclair GPIO18 RX
Tardi GPIO41 RX <- Eclair GPIO17 TX

Eclair GPIO4  -> Z1 Mouth, 208 pixels
Eclair GPIO5  -> Z2 Shoulder, 325 pixels
Eclair GPIO6  -> Z3 Midbody, 400 pixels
Eclair GPIO7  -> Z4 Rear, 300 pixels
Eclair GPIO8  -> Z5 Front legs, 300 pixels
Eclair GPIO9  -> Z6 Back legs, 300 pixels
Eclair GPIO10 -> Z7 Digestive, 75 pixels
```

Total: 1,908 pixels. Tardi, Eclair, and LED power must share ground. Direct
3.3 V GPIO-to-DIN is the selected but not yet physically proven electrical
path; use a suitable level shifter if the installed LEDs show marginal data.

## Required toolchains

| Target | Arduino-ESP32 | FastLED | Physical LEDs |
|---|---:|---:|---|
| Tardi | 3.3.10 | not linked | none |
| Eclair | 2.0.17 / IDF 4.4.7 | 3.9.20 | Z1-Z7, RMT4 |

FastLED 3.10.4 with Arduino-ESP32 3.3.10/IDF5 is not a valid Eclair7 RMT4
configuration. The sketch-local RMT4 selection ends in a missing factory
symbol; forcing it globally exposes IDF5 interface and `RMTMEM` failures.
FastLED 3.9.20 with core 2.0.17 is the verified seven-controller build.

The Eclair binary audit must find `ESP32RMTController`, `startNext()` and
`RMTMEM`, and must not find `ClocklessBlockingGeneric`. That proves driver
selection only. Physical testing must still confirm every lane, especially
the full 400-pixel Z3 multi-chunk path, wiring, power and signal integrity.

## Runtime safety

Tardi sends full state every 20 ms over 2,000,000-baud UART1. Eclair starts
black and forces all seven lanes black 500 ms after the last valid packet,
remaining black until communication recovers. Runtime output modes, validation
modes and the five-second startup check are Tardi-controlled; saved settings
remain on Tardi.

Authoritative detail:

- `firmware/eclair_controller/README.md`
- `docs/current_baseline.md`
- `docs/direct_led_output.md`
- `docs/gpio_schema.md`
- `docs/interaction_logic.md`
