# Interaction Logic

## Loop Ownership

The controller reads and debounces physical buttons, accepts interactions,
updates LED trigger windows, maintains FIRE pulse state, writes active-LOW FIRE
pins, renders LEDs, and services Serial/Web.

The LED engine never reads buttons and never touches FIRE pins.

## Normal Button Behavior

| Button | FIRE | LED |
|---:|---:|---|
| 1 | FIRE1 | Z1 Mouth |
| 2 | FIRE2 | Z2 Shoulder |
| 3 | FIRE3 | Z3 Midbody |
| 4 | FIRE4 | Z4 Rear |
| 5 | FIRE5 | Z5 Front legs |
| 6 | FIRE6 | Z6 Back legs |
| 7 | FIRE7 | Z7 Digestive |
| 8 | FIRE8 | none |

A new press starts a 100 ms FIRE pulse. While held, the matching FIRE output
repeats a 100 ms pulse every 1,000 ms. Release stops future repeats.

Accepted Button 1–7 press events activate their zones for the saved animation
duration. Zones then return to ambient rendering.

## Head Poof

Button 1 + Button 8 requests:

- FIRE9 while both buttons remain held, limited to 10 seconds;
- all seven LED zones active for the saved animation duration.

Releasing either button ends the FIRE9 request. LED duration is independent of
FIRE timing.

## Proof-of-Concept Overrides

All eight buttons held:

- FIRE1–FIRE9 pulse together for 500 ms once;
- normal repeats and Head Poof handling are suppressed while all remain held;
- release at least one button to re-arm.

Buttons 2 + 6 held:

- all wired LED zones render green;
- release either button to resume normal rendering;
- saved settings are unchanged.

## LED Startup

After LED initialization, a five-second moving hardware check runs before
Wi-Fi and normal loop processing. It uses temporary visible brightness and a
fixed nonzero speed without changing saved settings. FIRE outputs remain at
their initialized HIGH/idle level; normal button, FIRE, LED, Serial-command,
and web processing begins when the check finishes.

## Web Boundary

The web page edits LED appearance and duration only. It has no FIRE or relay
controls. Changes apply in RAM until saved.
