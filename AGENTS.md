# Tardi Controller working rules

## Critical safety baseline

The existing ESP32-S3 FIRE/button bench tests have already passed.

Do not change:
- FIRE GPIO logic
- FIRE polarity
- button pins
- OLED pins
- Big Poof cutoff behaviour

FIRE outputs are active-LOW:
- initialize HIGH
- pulse LOW to fire
- return HIGH to stop

## PixelBlaze reference rules

The folder `reference_only/pixelblaze/` is reference-only.

Do not modify anything inside:
- `reference_only/`

Do not copy PixelBlaze GPIO numbers into ESP32 firmware.
Do not port PixelBlaze fire/poofer logic.
Do not port PixelBlaze fire polarity.
Do not port generated `dist/` code.
Do not port archived old pattern files.

Use the PixelBlaze source only to understand LED layout, animation structure, and per-zone behaviour.

## Desired LED architecture

Recreate the PixelBlaze LED engine structure conceptually in ESP32/C++.

Do not build one massive `.ino`.

Keep `firmware/esp32_controller/esp32_controller.ino` as the existing controller coordinator.

Add LED code as compartmentalised files beside the sketch, using names that map clearly to the PixelBlaze source:

- `led_config.h`
- `led_layout.h`
- `led_animations.h/.cpp`
- `led_state.h/.cpp`
- `led_engine.h/.cpp`
- later zone files matching PixelBlaze zones:
  - `led_z1_mouth.h/.cpp`
  - `led_z2_shoulder.h/.cpp`
  - `led_z3_midbody.h/.cpp`
  - `led_z4_rear.h/.cpp`
  - `led_legs.h/.cpp`
  - `led_z7_digestive.h/.cpp`
  - `led_z8_stations.h/.cpp`

The LED engine must not read physical buttons directly.
The LED engine must not touch FIRE pins.

Existing controller logic owns:
- button debounce
- accepted button/FIRE triggers
- FIRE outputs
- Big Poof logic
- safety cutoffs
- OLED diagnostics
- serial logging

## ESP32 LED active-state model

Do not copy the PixelBlaze held-button model:

```text
zoneActive = buttonHeld && !latched
```

Use the ESP32 controller trigger-window model instead:

```text
zoneActive = now < ledActiveUntil[zone]
```

LED active state should be driven from accepted controller events / active windows, not raw PixelBlaze button-held behaviour.

## Pixelblaze Output Expander rules

The current physical LED output target is:

```text
ESP32-S3 -> UART -> Pixelblaze Output Expander -> LED zones
```

`PBDriverAdapter` is vendored locally under:

```text
firmware/esp32_controller/src/PBDriverAdapter/
```

`PBDriverAdapter` has a Tardi local patch allowing a caller-selected ESP32 TX pin:

```cpp
void PBDriverAdapter::begin(uint32_t uartFrequency, int8_t txPin);
```

Future real Output Expander UART settings:
- baud = `2000000`
- TX = `GPIO39`

Do not use board-labelled TX/RX / UART0 for the Output Expander.
Do not use GPIO16 because it is Button 6.
Do not use PBDriverAdapter's old default ESP32 TX GPIO23 for Tardi real output.

Real Output Expander output must remain disabled unless explicitly requested:

```cpp
ENABLE_REAL_PB_EXPANDER_OUTPUT = false
```

Codex must not flip this guard to true unless the task explicitly asks for California hardware validation.
India-side work should remain compile-only, simulator-based, or diagnostics-only.
Physical UART / expander / LED validation is California-side.

## Output Expander simulator

The Output Expander simulator is allowed and safe.

Expected healthy India-side simulator status:

```text
EXP REAL allowed=0 started=0 tx=39 baud=2000000
EXP SIM OK channels=8 pixels=2008 failed=0 ... byteOrder=GRB
```

The checksum varies with animation time/state and is not a fixed universal value.
