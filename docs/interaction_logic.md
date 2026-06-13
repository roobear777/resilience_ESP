````markdown
# Interaction Logic

This file describes the controller behavior in plain English.

For confirmed hardware baseline, electrical assumptions, LED architecture assumptions, output modes, and pin-mapping rules, see:

```text
docs/current_baseline.md
docs/gpio_schema.md
docs/pin_mapping.md
docs/led_output_expander.md
docs/led_animation_architecture.md
```

This file should focus on what the controller does during normal operation.

## 1. Core Loop

The controller repeatedly:

1. Reads the 8 active-HIGH button input signals.
2. Debounces the button readings.
3. Detects new button press events.
4. Updates the interaction state.
5. Starts any required FIRE pulses.
6. Updates active FIRE pulse timing.
7. Calculates FIRE1-FIRE9 output states.
8. Updates physical FIRE output pins if live mode is enabled.
9. Updates LED animation state.
10. Sends LED data to the Pixelblaze Output Expander if LED output is enabled.
11. Updates Serial debug.
12. Updates the OLED display.

Button input wiring is handled as direct ESP32 3.3V logic in the current preferred architecture:

```text
ESP32 3.3V -> button panel -> button return wire -> ESP32 GPIO input
```

The input relay layer is not part of the preferred new button input architecture.

Current LED output architecture assumes:

```text
ESP32-S3 -> UART -> standard Pixelblaze Output Expander v3.0 -> LED zones
```

The ESP32-S3 owns the interaction logic, FIRE logic, and LED animation logic.

The Output Expander is used as an 8-channel, 5V level-shifted LED output board.

## 2. Button Input Logic

There are 8 logical button inputs:

```text
Button 1
Button 2
Button 3
Button 4
Button 5
Button 6
Button 7
Button 8
```

Buttons use active-HIGH logic:

```text
GPIO HIGH = pressed / active
GPIO LOW  = released / inactive
```

Debounce time:

```text
30 ms
```

A new press event means:

```text
button was released / LOW
button becomes pressed / HIGH
```

The firmware tracks:

* raw button state
* debounced button state
* previous debounced button state
* new press events
* button press start time
* hold duration, for diagnostics or future behavior
* selected input pull-down mode, if reported in debug output

The interaction logic must not invert the button behavior.

Pull-down wiring only defines the released/inactive state.

It does not change the active-HIGH button logic.

## 3. Current GPIO Logic Summary

Detailed GPIO mapping is tracked in:

```text
docs/gpio_schema.md
```

Current temporary button allocation:

```text
Button 1 = GPIO4
Button 2 = GPIO5
Button 3 = GPIO6
Button 4 = GPIO7
Button 5 = GPIO15
Button 6 = GPIO16
Button 7 = GPIO17
Button 8 = GPIO18
```

Current temporary FIRE allocation:

```text
FIRE1 = GPIO8
FIRE2 = GPIO9
FIRE3 = GPIO10
FIRE4 = GPIO11
FIRE5 = GPIO12
FIRE6 = GPIO13
FIRE7 = GPIO14
FIRE8 = GPIO21
FIRE9 / Big Poof = GPIO47
```

Current OLED allocation:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
```

These are current bench-test assignments only.

Final California wiring may still change.

Interaction logic should always refer to logical names such as `Button 1`, `FIRE1`, and `FIRE9`, not scattered raw GPIO numbers.

## 4. Fire Output Logic

There are 9 logical FIRE outputs:

```text
FIRE1
FIRE2
FIRE3
FIRE4
FIRE5
FIRE6
FIRE7
FIRE8
FIRE9
```

FIRE1-FIRE8 are normal FIRE outputs.

FIRE9 is reserved for big poof.

A FIRE output means:

```text
ESP32-controlled signal -> relay channel -> poofer/solenoid circuit
```

Poofer relay outputs use active-LOW logic:

```text
GPIO HIGH = relay inactive
GPIO LOW  = relay triggered
```

All FIRE outputs should initialize HIGH during startup.

The ESP32 pulls an output LOW for the duration of a FIRE pulse and then returns it HIGH.

The ESP32 does not directly power the solenoid.

The FIRE output behavior is independent from the button input wiring simplification.

The FIRE output behavior is also independent from the LED Output Expander work.

## 5. Normal FIRE1-FIRE8 Logic

Normal FIRE1-FIRE8 logic is separate from big-poof logic.

For controller testing, the normal mapping is:

```text
Button 1 press -> FIRE1 pulse
Button 2 press -> FIRE2 pulse
Button 3 press -> FIRE3 pulse
Button 4 press -> FIRE4 pulse
Button 5 press -> FIRE5 pulse
Button 6 press -> FIRE6 pulse
Button 7 press -> FIRE7 pulse
Button 8 press -> FIRE8 pulse
```

Latest real firmware mapping test result:

```text
Button 1 -> FIRE1 = PASS
Button 2 -> FIRE2 = PASS
Button 3 -> FIRE3 = PASS
Button 4 -> FIRE4 = PASS
Button 5 -> FIRE5 = PASS
Button 6 -> FIRE6 = PASS
Button 7 -> FIRE7 = PASS
Button 8 -> FIRE8 = PASS
Button 1 + Button 8 -> FIRE9 / Big Poof = PASS
```

The final physical sculpture mapping will be documented separately when California wiring is confirmed.

Normal FIRE1-FIRE8 logic should not contain special FIRE9 / big-poof behavior.

## 6. Normal FIRE Pulse Behavior

Normal FIRE1-FIRE8 outputs use one-shot pulse behavior.

Behavior:

```text
Button press -> 500 ms FIRE pulse
Output switches LOW for the pulse duration
Output returns HIGH automatically after 500 ms
Releasing and pressing again can trigger another pulse
```

The normal FIRE1-FIRE8 pulse duration is configurable in firmware.

Current intended starting pulse duration:

```text
500 ms
```

This is the confirmed starting behavior for normal FIRE1-FIRE8 outputs.

It may still be tuned later after relay testing, flame testing, and California hardware review.

The intended behavior is one-shot 0.5 second firing.

This is safer than keeping a FIRE output active for the full time a button is held.

## 7. FIRE9 / Big Poof Logic

FIRE9 is reserved for big poof.

The current firmware scaffold uses this big-poof trigger:

```text
Button 1 + Button 8 pressed together -> FIRE9
```

This trigger rule should stay isolated in one firmware function, for example:

```cpp
bool isBigPoofRequested() {
  return debouncedButtonState[0] && debouncedButtonState[7];
}
```

This makes it easy to change the big-poof behavior later without rewriting normal FIRE1-FIRE8 logic.

Current behavior:

```text
Button 1 + Button 8 can trigger FIRE1, FIRE8, and FIRE9.
```

This means the big-poof trigger adds FIRE9 but does not override normal Button 1 and Button 8 behavior.

A later revision can change the big-poof trigger to activate FIRE9 only.

Possible future big-poof triggers could include:

```text
Button combination
Long press
Double press
Triple press
Dedicated button
Sequence of buttons
```

Those are future behavior decisions.

They should not affect normal FIRE1-FIRE8 logic.

## 8. FIRE9 Pulse Behavior

FIRE9 also uses one-shot pulse behavior.

Behavior:

```text
Big-poof trigger requested -> FIRE9 pulse
Output switches LOW for the pulse duration
Output returns HIGH automatically after its pulse duration
Releasing and triggering again can start another FIRE9 pulse
```

FIRE9 pulse duration is configurable separately from FIRE1-FIRE8.

Current FIRE9 pulse duration:

```text
500 ms
```

FIRE9 currently also uses 500 ms, but it remains separately configurable.

FIRE9 keeps its own timing state, separate from Button 1 and Button 8 timers.

## 9. Output Cutoffs

The normal output behavior is pulse-based, not hold-based.

Primary safety behavior:

```text
FIRE output automatically returns HIGH after its pulse duration
```

For normal FIRE1-FIRE8 outputs, that pulse duration is currently:

```text
500 ms
```

A longer cutoff remains as a backup guard against any output being held active unexpectedly.

Current backup cutoff:

```text
10 seconds
```

This cutoff is backup protection only.

It should never be reached during normal 500 ms pulse behavior.

If a future mode adds repeated poofs while a button is held, the cutoff rules must be reviewed.

## 10. LED Animation Logic

The current LED output architecture assumes:

```text
ESP32-S3 -> UART -> standard Pixelblaze Output Expander v3.0 -> LED zones
```

The ESP32-S3 owns the LED animation logic.

The LED system should respond to the same interaction state that controls FIRE1-FIRE9.

For example:

```text
Button press -> FIRE pulse requested
Button press -> matching LED zone animation requested
Big poof requested -> FIRE9 pulse requested
Big poof requested -> big-poof LED animation requested
```

The LED animation logic should use logical interaction names:

```text
Button 1
Button 2
FIRE1
FIRE9
LED zone 1
LED zone 8
Big Poof
```

It should not depend directly on raw GPIO numbers.

The exact LED zone mapping is still a separate LED architecture task.

## 11. LED Output Expander Hook

The standard Pixelblaze Output Expander v3.0 is treated as an LED output board, not as the main controller.

Current intended signal path:

```text
ESP32-S3 animation logic
 -> UART LED data
 -> Pixelblaze Output Expander v3.0
 -> 8 LED output channels
 -> LED zones
```

The Output Expander provides 8 level-shifted LED data outputs.

The Output Expander does not power LED strips.

The Output Expander does not replace LED power injection.

The LED strips still need a separate correctly sized 5V LED power system.

The interaction logic should only produce LED animation requests or LED zone state.

Low-level UART formatting and expander-specific output details belong in:

```text
docs/led_output_expander.md
docs/led_animation_architecture.md
```

## 12. Serial Debug Behavior

Serial debug is required.

Serial debug should show:

```text
button pressed/released states
debounced states
new press events
FIRE1-FIRE9 output states
whether FIRE9 big poof is requested
whether the firmware is in simulation mode or live mode
whether internal pull-down mode is enabled for the current bench setup
current LED animation requests or LED zone state
whether LED output is disabled, simulated, or being sent to the Output Expander
```

If pulse timing is active, Serial debug should also confirm:

```text
when a pulse starts
when a pulse ends
which FIRE output is pulsing
```

Serial debug may include GPIO numbers because it is a technical diagnostic channel.

## 13. OLED Display Behavior

The OLED display is required.

The OLED is a compact diagnostic display, not a decorative display.

The OLED should show the controller signal flow in plain language:

```text
mode
current state
input number
output number
LED zone / trigger
live output status
```

The main display states are:

```text
READY
FIRING
PULSE COMPLETE
BIG POOF
SAFETY STOP
```

### READY

Shown when no input and no output pulse is active.

Example:

```text
SIMULATOR MODE

READY
Input: -
Output: OFF
LED: -
No live output
```

### FIRING

Shown during an active 500 ms output pulse.

Simulator example:

```text
SIMULATOR MODE

FIRING
Input: 4
Output: 4
LED: 4
No live output
```

Live example:

```text
LIVE MODE

FIRING
Input: 4
Output: 4
LED: 4
Live output: ON
```

### PULSE COMPLETE

Shown when an input is still active after the 500 ms output pulse has finished.

This confirms that holding an input does not keep the output firing.

Example:

```text
LIVE MODE

PULSE COMPLETE
Input: 4
Output: OFF
LED: 4
Live output: OFF
```

### BIG POOF

Shown when the Button 1 + Button 8 trigger is active or FIRE9 is active.

Example:

```text
LIVE MODE

BIG POOF
Input: 1+8
Output: 1 8 9
LED: BIG
Live output: ON
```

### SAFETY STOP

Shown only if a safety cutoff or unexpected output condition is detected.

Example:

```text
SAFETY STOP

Output 4
forced OFF
```

The OLED should not permanently show:

```text
raw input masks
GPIO pin numbers
pull-down mode
Pixelblaze internals
Output Expander internals
Plan A / Plan B labels
timer countdowns
decorative graphics
```

Pull-down mode, raw inputs, pulse start/end events, UART details, and detailed timing diagnostics belong in Serial debug unless a later detail screen is added.

The OLED should use neutral `LED` wording so the same display language can still work if the LED output architecture changes later.

The OLED should not hardcode Pixelblaze-specific or Output-Expander-specific wording into the main display.
````
