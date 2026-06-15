# California LED Output Expander Validation

This guide is for Zael's California physical validation of:

```text
ESP32-S3 -> PBDriverAdapter -> Pixelblaze Output Expander -> LEDs
```

It opens one gate at a time. If a step fails, stop and record what happened.

## Quick Start

1. Get the latest validation code.
2. Upload the California validation firmware to the ESP32.
3. Connect the ESP32, Output Expander, and LED setup.
4. Run the guided Terminal script:

   ```sh
   cd ~/Documents/tardi-controller
   python3 tools/validate_led_expander.py
   ```

5. Follow the prompts.
6. If a step fails, stop.
7. Send the newest `.txt` log from `validation_logs/` back to Roopert.

If Zael already knows the ESP32 USB Serial port, pass it explicitly:

```sh
python3 tools/validate_led_expander.py --port /dev/cu.usbserial-0001
```

## What This Test Proves

- ESP32 sends real UART LED data on GPIO39.
- The Output Expander receives/responds.
- Channel wiring can be checked.
- Full animation is only tested after basic output works.

## Before You Start

- California validation firmware has been uploaded.
- ESP32 is connected over USB.
- Output Expander is powered correctly.
- LED power is safe.
- ESP32 GND is connected to Output Expander GND.
- ESP32 GPIO39 is connected to Output Expander data/UART input.
- GPIO39 is not connected to 5V.
- Real LEDs are not expected to animate immediately on boot.

## Safety Model

- "Real output allowed" means this firmware is permitted to talk to the Output Expander.
- "LED mode OFF" means it is not sending active LED test or animation data yet.
- The validation firmware may allow real output, but it should still boot in LED mode OFF.
- Nothing should start animating just from power-up.
- The Terminal script opens the next gate only after Zael confirms the previous one worked.
- The ESP32 cannot automatically know whether the physical wiring is correct. Zael must visually confirm real LED behaviour.

## Step-by-Step Validation

### Step 1 - Boot Safely

Expected:

- ESP32 boots.
- LEDs do not start animating unexpectedly.
- OLED/Serial shows a sensible setup/status state.
- `led status` reports mode OFF.

The guided script sends/checks:

```text
led status
```

If this is wrong:

- stop
- record what happened
- do not continue to LED tests

### Step 2 - Solid Output Test

Command:

```text
led solid
```

Expected:

- connected LED zones show a dim simple test output.

If nothing lights:

- stop
- record what happened
- do not continue to channel tests

### Step 3 - Channel Tests

Commands:

```text
led ch 0
led ch 1
led ch 2
led ch 3
led ch 4
led ch 5
led ch 6
led ch 7
```

Expected:

- only the selected expander channel / physical zone lights.

For each channel, Zael should record:

- which physical zone lit
- whether only one zone lit
- whether colours looked sensible
- whether nothing happened

If the wrong zone lights, multiple zones light, colours look swapped, or nothing happens:

- stop before animation
- record the details

### Step 4 - Animation Test

Command:

```text
led animation
```

Only run this if solid and channel tests are sensible.

Expected:

- Tardi animation output runs.

If it looks wrong:

- record what happened

### Step 5 - Off Test

Command:

```text
led off
```

Expected:

- active validation/animation output stops.

## If Something Fails

Do not keep opening more gates.

Record:

- command used
- expected result
- actual result
- which physical zone lit, if any
- whether the wrong zone lit
- whether multiple zones lit
- whether colours looked swapped
- whether nothing happened
- any photos/videos if helpful

## Guided Terminal Script

The guided script is preferred.

Run:

```sh
cd ~/Documents/tardi-controller
python3 tools/validate_led_expander.py
```

If needed:

```sh
python3 tools/validate_led_expander.py --port /dev/cu.usbserial-0001
```

The script:

- does not upload firmware
- does not edit source code
- does not enable real output
- sends existing `led ...` Serial commands
- asks Zael what physically happened
- stops when a gate fails
- saves a log

Serial baud notes:

- USB Serial guide baud: `115200`
- Output Expander real LED UART baud: `2000000`

## Manual Serial Commands Fallback

The guided script is preferred. These commands are still useful for manual fallback or quick checks.

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

## Validation Logs

The guided script saves logs in:

```text
validation_logs/
```

At the end of a run, the script prints the exact log path.

Zael should send the newest `.txt` file back to Roopert manually.

Do not commit validation logs to git.

Mac helper:

```sh
open validation_logs
```

## Simple Mental Model

- Git / ZIP gets new code to Zael.
- Arduino upload puts that code on the ESP32.
- Terminal script sends live Serial commands.
- Zael visually confirms the real LEDs.
- The log goes back to Roopert.
- Roopert makes fixes and sends a new version if needed.
