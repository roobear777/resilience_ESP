# California LED Output Expander Validation

This guide is for Zael's California physical validation of:

```text
ESP32-S3 -> PBDriverAdapter -> Pixelblaze Output Expander -> LEDs
```

It opens one gate at a time. If a step fails, stop and record what happened.

## Prerequisites

The guided validation script requires Python 3 and the `pyserial` package on the Mac running Terminal.

Install `pyserial` once with:

```sh
python3 -m pip install pyserial
```

On macOS, seeing `Defaulting to user installation because normal site-packages is not writeable` is normal and is not an error.

Do not use `sudo` unless someone deliberately knows why they need it.

After installing, run the validation script again:

```sh
python3 tools/validate_led_expander.py
```

This Python package is only for the Mac script talking to the ESP32 over USB Serial. USB Serial uses `115200`; the Output Expander's real LED UART uses `2000000`.

## Quick Start

1. Get the latest validation code.
2. Upload the California validation firmware to the ESP32.
3. Connect the ESP32, Output Expander, and LED setup.
4. Close Arduino Serial Monitor and Serial Plotter. The Arduino IDE itself may stay open.
5. Run the guided Terminal script:

   ```sh
   cd ~/Documents/tardi-controller
   python3 tools/validate_led_expander.py
   ```

6. Follow the prompts.
7. If a step fails, stop.
8. Send the newest `.txt` log from `validation_logs/` back to Roopert.

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
- Arduino Serial Monitor and Serial Plotter are closed before running the guided script. Only one program can use the ESP32 USB Serial port at a time.
- Target hardware is Zael's photographed ElectroMage Pixelblaze Output Expander v3.0 board.
- Output Expander is powered correctly.
- LED power is safe.
- ESP32 GND is connected to Output Expander GND.
- ESP32 GPIO39 is connected to Output Expander data/UART input.
- GPIO39 is not connected to 5V.
- Real LEDs are not expected to animate immediately on boot.

Capacity note: use the current ElectroMage product page for this board as the capacity reference. It documents up to 800 RGB pixels per channel for WS2812 / NeoPixel-style output. Tardi's largest planned channel is Ch3 at 400 pixels, so the planned channel lengths are within that documented capacity. Physical wiring, colour order, grounding, and real LED behaviour still need this California validation pass.

## Safety Model

- "Real output allowed" means this firmware is permitted to talk to the Output Expander.
- "LED mode OFF" means it is not sending active LED test or animation data yet.
- The current California validation firmware allows real output, but it should still boot in LED mode OFF.
- Nothing should start animating just from power-up.
- The Terminal script opens the next gate only after Zael confirms the previous one worked.
- The ESP32 cannot automatically know whether the physical wiring is correct. Zael must visually confirm real LED behaviour.
- In this guide, "LEDs" means the real LED strings connected to the Output Expander, not ESP32 onboard LEDs.
- In firmware Serial diagnostics, `Outputs=ON` means FIRE outputs are enabled. It does not mean LED UART output is on.

## Step-by-Step Validation

### Step 1 - Boot Safely

Expected:

- ESP32 boots.
- OLED/Serial shows a sensible setup/status state.
- `led status` reports mode OFF.

The guided script first asks Zael to reset or power-cycle the ESP32 while the Output Expander is connected and the real LED strings are powered.

India USB-only dry run:

- if no Output Expander and no real LED strings are connected, answer `n` to the flicker question.
- for future USB-only dry runs without Output Expander hardware, temporarily set `ENABLE_REAL_PB_EXPANDER_OUTPUT=false` before uploading.
- this only tests the Python / USB Serial / safety flow.
- it does not prove GPIO39 electrical output, 2M UART timing, Output Expander response, real LEDs, colour order, or channel mapping.

Expected:

- before the script sends any `led` command, no real sculpture LEDs flicker, flash, or animate.

If any real sculpture LEDs flicker, flash, or animate:

- stop
- record which channels/zones flickered
- do not continue to the solid test
- send the validation log back to Roopert

If there is no boot/reset flicker, the guided script sends/checks:

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
- do not continue to colour or channel tests

### Step 3 - Colour-Order Tests

Commands:

```text
led red
led green
led blue
```

Expected:

- connected LED zones show dim red, then dim green, then dim blue.

These low-brightness all-channel tests confirm that red, green, and blue are not swapped on the real LED strings. The firmware currently packs Output Expander bytes as GRB, but the ESP32 cannot know whether the physical LED colour order is correct without Zael visually confirming it.

If a colour is wrong:

- stop before channel tests and animation
- record which command was used
- record what colour actually appeared

### Step 4 - Channel Tests

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

### Step 5 - Animation Test

Command:

```text
led animation
```

Only run this if solid, colour-order, and channel tests are sensible.

Expected:

- Tardi animation output runs.

If it looks wrong:

- record what happened

### Step 6 - Off Test

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

Before running it, close Arduino Serial Monitor and Serial Plotter. The Arduino IDE itself may stay open.

Only one program can use the ESP32 USB Serial port at a time. If the script reports `Resource busy`, close Serial Monitor / Serial Plotter and retry.

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
- checks for boot/reset flicker before sending the first `led` command
- keeps full raw Serial output in the saved log
- shows a concise parsed summary in Terminal when possible
- asks Zael what physically happened
- stops when a gate fails
- saves a log

Serial baud notes:

- USB Serial guide baud: `115200`
- Output Expander real LED UART baud: `2000000`

## Advanced Optional Tool Check

If a logic analyzer or oscilloscope is available, GPIO39 can be probed during ESP32 reset/power-cycle to look for unwanted activity before firmware setup takes over.

This is optional. The main validation flow is still based on Zael visually checking whether real sculpture LED strings flicker, flash, or animate before the script sends any `led` command.

## Manual Serial Commands Fallback

The guided script is preferred. These commands are still useful for manual fallback or quick checks.

Available USB Serial commands:

```text
led status
led help
led off
led solid
led red
led green
led blue
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
- `led red`, `led green`, `led blue`: low-brightness all-channel colour-order tests.
- `led ch N`: low-brightness selected-channel test; other channels should be off.
- `led animation`: full animation output; run only after solid, colour-order, and channel tests pass.

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
