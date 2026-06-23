# Interaction Logic

This file describes what the controller does during normal live operation.

## Core Loop

The controller repeatedly:

1. Reads 8 active-HIGH button inputs.
2. Debounces button readings.
3. Detects accepted new press events.
4. Updates FIRE pulse state.
5. Updates Big Poof state.
6. Updates LED trigger windows.
7. Writes active-LOW FIRE outputs.
8. Renders and sends LED data to the Output Expander.
9. Updates OLED, Serial diagnostics, and web server handling.

## Buttons

Buttons use active-HIGH logic:

```text
LOW  = released
HIGH = pressed
```

The live wiring uses external 10k pull-downs. Firmware uses normal input mode for the button pins.

## Normal FIRE Behaviour

Normal button-to-FIRE mapping:

```text
Button 1 -> FIRE1
Button 2 -> FIRE2
Button 3 -> FIRE3
Button 4 -> FIRE4
Button 5 -> FIRE5
Button 6 -> FIRE6
Button 7 -> FIRE7
Button 8 -> FIRE8
```

FIRE outputs are active-LOW:

```text
HIGH = idle / inactive
LOW  = trigger / active
```

Normal FIRE1-FIRE8 pulse duration:

```text
500 ms
```

The normal FIRE pulse is separate from the LED animation duration.

## Normal LED Behaviour

Ambient LED animation starts automatically after boot.

Normal button-to-LED mapping:

```text
Button 1 -> Z1 / Mouth
Button 2 -> Z2 / Shoulder
Button 3 -> Z3 / Midbody
Button 4 -> Z4 / Rear
Button 5 -> Z5 / Front legs
Button 6 -> Z6 / Back legs
Button 7 -> Z7 / Digestive
```

Button 8 alone does not trigger an independent LED zone. It still triggers FIRE8.

Z8 is the button-station LED zone. It mirrors/summarizes Z1-Z7 activity and participates in all-zone Big Poof.

Triggered LED zones use the saved global animation duration. Default:

```text
10 seconds
```

When that duration expires, each zone returns to ambient rendering.

## Big Poof

Big Poof is the Button 1 + Button 8 combo.

When the combo is accepted:

```text
FIRE9 / Big Poof triggers
all LED zones Z1-Z8 animate together as one synchronized event
```

Big Poof FIRE output:

```text
FIRE9 active while combo is held, with 10 second maximum cutoff
```

Big Poof LED animation:

```text
all zones active for the saved global animation duration
then all zones return to ambient
```

The Big Poof LED duration and FIRE cutoff are separate settings/behaviours.

## Web Controller

The web controller changes LED look/feel settings only.

It does not provide FIRE, relay, or hardware test controls.

Web setting changes apply live in RAM. `SAVE` persists them to flash. `RESET` restores defaults in RAM until saved.

## OLED Model

The OLED controller page prioritizes live controller state:

```text
SIMULATED or LIVE
READY / FIRING / PULSE COMPLETE / BIG POOF
Input: ...
FIRE: ...
LED: ...
```

The setup page is for LED UART/web setup status and must not claim that physical LEDs or wiring are proven good.

## Safety Boundaries

Do not change these without an explicit safety task:

- FIRE GPIO assignments
- FIRE active-LOW polarity
- normal 500 ms FIRE pulse
- Big Poof 10 second FIRE cutoff
- button mappings
- LED channel mapping
- GPIO39 Output Expander UART setting
