# California LED Output Expander Validation

This guide is for Zael's California physical validation of the real LED output path:

```text
ESP32-S3 -> PBDriverAdapter -> Pixelblaze Output Expander -> LEDs
```

The test proves, one gate at a time:

- ESP32 can send real UART LED data on GPIO39.
- The firmware can talk to the Pixelblaze Output Expander.
- The Output Expander responds.
- LED channel wiring can be checked.
- Full animation is tested only after simple validation passes.

## Safety Model

The normal/default firmware keeps real LED output disabled.

For California, a special validation build may allow real output. Even then, the firmware should still boot in `LED_OUTPUT_OFF`, so nothing should start animating immediately on power-up.

Real output allowed means the firmware is permitted to talk to the Output Expander. LED mode OFF means it is not sending active LED test or animation data yet.

The Terminal guide and USB Serial commands open each test step deliberately.

## Hardware Connections

Expected validation wiring:

- ESP32 GND -> Output Expander GND
- ESP32 GPIO39 -> Output Expander data/UART input
- Output Expander powered correctly for the LED setup
- LED power and grounds handled safely

Warnings:

- Do not connect GPIO39 to 5V.
- ESP32 and Output Expander must share ground.
- The ESP32 cannot automatically know whether the expander received data. Zael must visually confirm LED behaviour.

## Upload And Startup

Zael uploads the California validation firmware once.

Expected startup:

- ESP32 boots.
- LED mode is OFF.
- Real LEDs should not start animating immediately.
- OLED/Serial should indicate the current LED output state.
- `led status` should be the first command to check status.

## Manual Serial Commands

Available USB Serial commands:

```text
led status
led help
led off
led solid
led ch 0
led ch 1
led ch 2
led ch 3
led ch 4
led ch 5
led ch 6
led ch 7
led animation
```

Command meanings:

- `led status`: prints mode, real-output permission, started state, TX pin, and baud.
- `led help`: prints the command list.
- `led off`: stops active LED output and returns to OFF mode.
- `led solid`: low-brightness all-channel validation test.
- `led ch N`: low-brightness selected-channel test; other channels should be off.
- `led animation`: full animation output; run only after solid and channel tests pass.

## Recommended Validation Order

### A. Safe Boot Check

- Upload validation firmware.
- Confirm LEDs do not start unexpectedly.
- Run `led status`.
- Confirm mode is OFF before proceeding.

### B. Solid Test

- Run `led solid`.
- Expected: connected LED zones show a dim simple test output.
- If nothing lights, stop and record what happened.

### C. Channel Tests

- Run `led ch 0` through `led ch 7`, one at a time.
- Expected: only the selected expander channel / physical zone lights.
- Record which physical zone lights for each channel.
- If a channel lights the wrong zone, record it and stop before animation.
- If colours look wrong, record that too.

### D. Animation Test

- Only run `led animation` if solid and channel tests are sensible.
- Expected: Tardi animation output runs.
- If it looks wrong, record what happened.

### E. Off Test

- Run `led off`.
- Expected: active validation/animation output stops.

## What To Do On Failure

If a step fails, do not keep opening more gates.

Record:

- command used
- what was expected
- what actually happened
- which physical zone lit, if any
- whether the wrong zone lit
- whether multiple zones lit
- whether colours looked swapped
- whether nothing happened

## Validation Logs

Planned logging path:

```text
validation_logs/
```

The future Terminal guide script will save logs there. Logs are local evidence files, not source code. Do not commit validation logs to git.

Send the newest `.txt` validation log back to Roopert manually.

Mac helper command:

```sh
open validation_logs
```

## Simple Mental Model

- Git / ZIP gets new code to Zael.
- Arduino upload puts that code on the ESP32.
- Serial commands change the live runtime LED mode.
- Zael visually confirms hardware behaviour.
- Validation log / notes go back to Roopert for fixes.
