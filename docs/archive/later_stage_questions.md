# Later Stage Questions

This file records decisions and questions that do not block the current ESP32 firmware scaffold.

For confirmed baseline details, see:

```text
docs/current_baseline.md
docs/interaction_logic.md
docs/pin_mapping.md
```

This file should only contain unresolved decisions, later-stage questions, and future integration notes.

## Question Sections

1. ESP32 Firmware Decisions
2. Button-to-FIRE Behavior
3. FIRE9 / Big Poof Behavior
4. Questions for Zael / California Hardware
5. Questions for Mehrdad / LED / PixelBlaze
6. LED Integration Questions

## 1. ESP32 Firmware Decisions

The current scaffold already confirms:

```text
Normal FIRE1-FIRE8 outputs use one-shot 500 ms pulses.
Holding a button does not keep FIRE active.
FIRE9 currently also uses a separately configurable 500 ms pulse.
A 10 second output cutoff remains as a backup guard only.
Hold-based fire is not the current baseline.
```

Still to decide later:

```text

```text
Should physical California testing change the 500 ms normal FIRE1-FIRE8 pulse duration?
Should FIRE9 / big poof use a different pulse duration from normal FIRE1-FIRE8?
Should FIRE9 have extra limits or cooldown compared with FIRE1-FIRE8?
Should repeated taps produce repeated poofs with no cooldown?
Should holding a button later create a rhythm/pattern?
Should rhythm mode be added later, or left out for the first build?
```

## 2. Button-to-FIRE Behavior

The current temporary firmware mapping is one-to-one:

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

Questions for later:

```text
Current bench-tested baseline:

```text
Button 1 triggers FIRE1
Button 2 triggers FIRE2
Button 3 triggers FIRE3
Button 4 triggers FIRE4
Button 5 triggers FIRE5
Button 6 triggers FIRE6
Button 7 triggers FIRE7
Button 8 triggers FIRE8
Each trigger creates one 500 ms active-LOW pulse
```

Questions for later:

```text
Should any final sculpture button trigger multiple FIRE outputs?
Should any final sculpture button be LED-only?
Should any final sculpture button be FIRE-only?
Should button behavior change depending on mode?
Should the final physical sculpture mapping differ from the current one-to-one bench mapping?
```

For now, keep the bench-tested one-to-one 500 ms pulse mapping until a better interaction design is chosen.

## 3. FIRE9 / Big Poof Behavior

FIRE9 is reserved for big poof.

Current bench-tested firmware trigger:

```text
Button 1 + Button 8 pressed together -> FIRE9 / Big Poof pulse
```

This rule should stay isolated in:

```cpp
bool isBigPoofRequested()
```

Questions for later:

```text
What should finally trigger FIRE9?
Should FIRE9 be triggered by a button combination?
Should FIRE9 be triggered by a long press?
Should FIRE9 be triggered by double press or triple press?
Should FIRE9 be triggered by a button sequence?
Should FIRE9 require multiple participants?
Should FIRE9 be disabled during some modes?
Should FIRE9 require a cooldown between pulses?
Should FIRE9 keep the current 500 ms pulse duration or use a different duration?
```

Current bench-tested behavior:

```text
Button 1 + Button 8 can trigger FIRE1, FIRE8, and FIRE9.
```

This happens because Button 1 and Button 8 still create their normal one-shot pulses, and the combo also starts a FIRE9 pulse.

Question to decide:

```text
Should big poof add FIRE9 on top of normal outputs?
Or should big poof override normal outputs and activate FIRE9 only?
```

FIRE9 should keep its own pulse timing, separate from Button 1 and Button 8 timing.

## 4. Questions for Zael / California Hardware

These are physical hardware questions for the California build.

### ESP32 Pin Wiring

```text
Which ESP32-S3 GPIO pins will be wired to Button 1-8?
Which ESP32-S3 GPIO pins will be wired to FIRE1-FIRE9?
Are any ESP32 pins unavailable or unsafe to use on the selected board?
Will the India and California ESP32 boards be the exact same model?
Should OLED pins be reserved before final FIRE/button wiring?
```

### Button Input Wiring

Current preferred direction:

```text
ESP32 3.3V -> shared 3.3V wire to button panel
button press -> return wire back to ESP32 GPIO input
external pull-down -> released input held LOW
```

Questions for California wiring:

```text
Can the physical button panel be rewired for direct ESP32 3.3V button logic?
What cable type will be used between ESP32 and the button panel?
How many spare wires should be included in the multi-core cable?
Where should the external 10kΩ pull-downs physically live?
Should pull-downs be individual resistors or a resistor array?
Are button signals clean enough for 30 ms debounce?
Does the button cable length create noise that needs filtering or shielding?
Should the button cable include shared GND?
How will shared 3.3V and shared GND be distributed at the button panel?
```

Important rule:

```text
ESP32 GPIO inputs must not receive 5V.
```

Input relays are no longer part of the preferred input architecture.

Question only if direct button wiring fails:

```text
Is there any remaining reason to keep input relays between the buttons and ESP32?
```

### Relay / Output Interface

```text
Are the existing output relay inputs compatible with 3.3V ESP32 GPIO logic?
Are output relay inputs active-HIGH or active-LOW?
What input current is required per relay channel?
Does the ESP32 need shared ground with the relay input side?
Are level shifters, buffers, or driver boards required?
Will a 500 ms pulse visibly trigger the relay/indicator?
Is there a relay response delay that affects pulse duration?
```

### OLED Hardware

```text
OLED model
display size
I2C or SPI
SDA pin
SCL pin
I2C address
required Arduino library
```

OLED is part of the current baseline, but implementation waits until the exact display details are confirmed.

## 5. Questions for Mehrdad / LED / PixelBlaze

These questions matter while the LED architecture is still unresolved.

### LED Controller Role

```text
Will PixelBlaze continue controlling LEDs for the next stage?
Will PixelBlaze be removed later?
Is direct ESP32 LED control feasible at the expected LED scale?
What is the actual LED count?
Is the LED scale approximately 2400 LEDs, or has that changed?
How are the LEDs currently divided into channels or zones?
What role does the existing PixelBlaze output expander play?
Would removing PixelBlaze also require replacing the output expander role?
```

### PixelBlaze Role If Kept

```text
Will PixelBlaze receive simple zone signals from ESP32?
Will PixelBlaze still read any buttons directly?
Will PixelBlaze still control any poofers directly?
Can poofer logic be removed from PixelBlaze code?
Can PixelBlaze be reduced to LED animation only?
```

### PixelBlaze Input from ESP32

```text
How many ESP32-to-PixelBlaze signals are needed?
Should ESP32 send one signal per button/zone?
Should ESP32 send one combined mode signal?
Should ESP32 send short trigger pulses or held zone-active signals to PixelBlaze?
What voltage does PixelBlaze expect on GPIO input?
Does PixelBlaze need 3.3V logic only?
Does ESP32 need protection or level shifting before PixelBlaze input?
```

### PixelBlaze Code Simplification

```text
Can PixelBlaze be simplified to ambient mode plus active zone mode?
Can PixelBlaze ignore all fire logic?
Can PixelBlaze treat ESP32 inputs as clean zone activation signals?
Should PixelBlaze animation be triggered by button press pulses or by active/held states?
Can current PixelBlaze animation code be reduced without breaking useful LED behavior?
```

## 6. LED Integration Questions

These questions affect how ESP32 and the final LED system work together.

Current controller direction:

```text
ESP32 = main button and FIRE logic brain
LED architecture = unresolved
```

Possible future directions:

```text
Option A: ESP32 handles buttons and FIRE outputs; PixelBlaze handles LEDs.
Option B: ESP32 handles buttons, FIRE outputs, and sends simple LED triggers to PixelBlaze.
Option C: ESP32 replaces PixelBlaze entirely and controls LEDs directly.
Option D: ESP32 sends LED triggers to another LED controller or interface.
```

Questions to resolve later:

```text
What is the minimum LED signal the ESP32 needs to send?
Should ESP32 send LED triggers at the same time as FIRE pulses?
Should LED behavior follow button press, button hold, or FIRE pulse timing?
If PixelBlaze remains, should ESP32 and PixelBlaze share ground?
If PixelBlaze remains, should PixelBlaze and ESP32 have separate power supplies?
If ESP32 drives LEDs directly, how will LED power injection and high current be handled?
How will we test LED communication without the full sculpture?
What is the fallback if PixelBlaze becomes too difficult to integrate?
What is the fallback if direct ESP32 LED control becomes too complex?
```

For now, the ESP32 firmware should remain modular:

```text
button reading
button press event detection
normal FIRE pulse logic
big poof trigger logic
FIRE pulse timing
Serial debug
OLED debug
future LED output hooks
```

Do not hardcode PixelBlaze assumptions into the core button and FIRE logic yet.