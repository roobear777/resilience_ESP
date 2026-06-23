# Tardi Controller — PixelBlaze LED Behaviour Consolidation

## Purpose

This document summarises the current PixelBlaze LED behaviour so it can later be ported to an ESP32/C++ LED engine.

This is a file-understanding and architecture-decision document only. It is not an implementation prompt.

The PixelBlaze folder is reference only. Do not modify it.

---

## ESP32 baseline that must not change

The existing ESP32-S3 FIRE/button bench tests have already passed.

Do not change:

* FIRE GPIO logic
* FIRE polarity
* button pins
* OLED pins
* Big Poof cutoff behaviour

Current ESP32 FIRE output rule:

```text
FIRE outputs are active-LOW.
Initialize HIGH.
Pulse LOW to fire.
Return HIGH to stop.
```

The old PixelBlaze fire/poofer code must not override this.

---

## Files reviewed

PixelBlaze source files:

```text
pixelblaze/src/config.js
pixelblaze/src/layout.js
pixelblaze/src/main.js
pixelblaze/src/buttons.js
pixelblaze/src/zone-state.js
pixelblaze/src/lib/animations.js
pixelblaze/src/zones/z1-mouth.js
pixelblaze/src/zones/z2-shoulder.js
pixelblaze/src/zones/z3-midbody.js
pixelblaze/src/zones/z4-rear.js
pixelblaze/src/zones/legs.js
pixelblaze/src/zones/z7-digestive.js
pixelblaze/src/zones/z8-string-lights.js
pixelblaze/src/main-pressure.js
pixelblaze/src/fire/config.js
pixelblaze/src/fire/poofers.js
pixelblaze/build.js
pixelblaze/README.md
pixelblaze/CONTEXT.md
```

Important repo rule:

```text
pixelblaze/src/ is source of truth.
pixelblaze/dist/ is generated.
Do not edit dist/ directly.
```

---

## Build variants

`pixelblaze/build.js` generates three deployable PixelBlaze patterns:

```text
master-full.js
- buttons + state machine + fire + LEDs

master-led-only.js
- same LED/button behaviour
- fire code stripped

pressure-test.js
- all zones forced active
- no buttons
- no fire
- useful burn-in / power / visual test reference
```

For ESP32 planning, the most relevant reference behaviour is:

```text
src/main.js
src/main-pressure.js
src/zones/*
src/config.js
src/layout.js
src/lib/animations.js
```

The fire folder is historical/reference only.

---

## Overall LED architecture

PixelBlaze uses this frame structure:

```text
beforeRender(delta)
→ pollButtons(delta)
→ updateZoneStates(delta)
→ advance animation timers
→ update old PixelBlaze fire logic
→ write old PixelBlaze fire pins

render(index)
→ dispatch each pixel index to the correct zone renderer
```

For ESP32, the useful concept is:

```text
frame update
→ update LED state
→ render each zone into LED buffers
→ send LED data to outputs
```

The PixelBlaze syntax itself should not be ported directly.

---

## Exact zone / output / pixel map

Total LED pixels:

```text
2008
```

Pixel index ranges in PixelBlaze render order:

```text
Z1:    0 –  207   = 208 px
Z2:  208 –  532   = 325 px
Z3:  533 –  932   = 400 px
Z4:  933 – 1232   = 300 px
Z5: 1233 – 1532   = 300 px
Z6: 1533 – 1832   = 300 px
Z7: 1833 – 1907   =  75 px
Z8: 1908 – 2007   = 100 px
```

Output Expander channel map:

```text
OE Ch0 = Z8 = 100 px
OE Ch1 = Z1 = 208 px
OE Ch2 = Z2 = 325 px
OE Ch3 = Z3 = 400 px
OE Ch4 = Z4 = 300 px
OE Ch5 = Z5 = 300 px
OE Ch6 = Z6 = 300 px
OE Ch7 = Z7 =  75 px
```

Important:

```text
Logical PixelBlaze render order:
Z1, Z2, Z3, Z4, Z5, Z6, Z7, Z8

Physical Output Expander order:
Z8, Z1, Z2, Z3, Z4, Z5, Z6, Z7
```

The ESP32 port must preserve the physical output order, not just the logical render order.

---

## LED hardware notes

From the project docs:

```text
Z1, Z2, Z5, Z6:
WS2812B strips

Z3, Z4, Z7:
WS2811 spools

Z7:
75-pixel signal, with 7 physical strands wired in parallel
```

Implication for ESP32 port:

```text
Do not assume every output uses the same LED chipset or physical wiring style.
```

---

## PixelBlaze button / active-state model

PixelBlaze uses 7 buttons:

```text
Button 1 → zoneActive[0]
Button 2 → zoneActive[1]
Button 3 → zoneActive[2]
Button 4 → zoneActive[3]
Button 5 → zoneActive[4]
Button 6 → zoneActive[5]
Button 7 → zoneActive[6]
```

PixelBlaze button inputs are active-HIGH:

```text
Pressed = GPIO HIGH
Released = GPIO LOW via external 10K pulldown
Debounce = 30 ms
```

PixelBlaze GPIO numbers are reference only and must not be copied into the ESP32 controller firmware.

PixelBlaze zone-state rule:

```text
Button held:
- zone hold timer increments
- zone active unless latched off

Button held longer than 10 seconds:
- zone latches off

Button released:
- hold timer clears
- latch clears
- zone returns to ambient
```

In the PixelBlaze files this comes from:

```text
buttons.js
→ reads buttonStates[i]

zone-state.js
→ zoneActive[i] = button held AND not latched

main.js
→ zoneActive[i] selects active animation vs ambient animation
```

Important mismatch with current ESP32 controller:

```text
PixelBlaze has 7 LED buttons.
Current ESP32 controller has 8 tested buttons.

PixelBlaze payoff = all 7 buttons held.
ESP32 Big Poof = Button 1 + Button 8.
```

So PixelBlaze button logic is useful as a reference, but it should not override the ESP32 controller baseline.

---

## ESP32 active-animation decision

Do not copy the PixelBlaze held-button/latch model directly.

PixelBlaze behaviour is:

```text
button held = active animation runs
button held for 10 seconds = active animation latches off
button released = latch/timer resets
```

For the ESP32 port, the LED engine should instead use a trigger-window model:

```text
accepted button/fire trigger = active LED animation runs for 10 seconds
accepted repeat trigger = active LED timer extends to 10 seconds from that new trigger
no repeat trigger = active LED animation ends after 10 seconds
```

Conceptually:

```text
zoneActive = now < ledActiveUntil[zone]
```

When the existing controller logic accepts a real trigger:

```text
ledActiveUntil[zone] = now + 10000
```

This means the LED active animation is tied to accepted controller events, not raw button-held state.

This is intentional because the ESP32 controller already owns:

```text
button debounce
button validity
FIRE triggering
FIRE safety cutoff
Big Poof logic
OLED diagnostics
serial logging
```

The LED engine should not independently reinterpret physical button holds or import the old PixelBlaze 10-second latch behaviour.

Porting rule:

```text
Do not port PixelBlaze zoneActive = buttonHeld && !latched as-is.

Instead, port the animation behaviours and drive their active/ambient selection from ESP32 LED trigger timers.
```

Initial LED trigger model:

```text
Accepted Button 1 trigger → LED Z1 active window extended
Accepted Button 2 trigger → LED Z2 active window extended
Accepted Button 3 trigger → LED Z3 active window extended
Accepted Button 4 trigger → LED Z4 active window extended
Accepted Button 5 trigger → LED leg active window extended
Accepted Button 6 trigger → LED leg active window extended
Accepted Button 7 trigger → LED Z7 active window extended
Button 8 → no PixelBlaze LED equivalent yet; Big Poof/fire-only unless a new LED role is confirmed
```

Important leg note:

```text
PixelBlaze treats Z5 and Z6 as one shared 8-leg animation split over two output channels.
The exact ESP32 button-to-leg mapping still needs confirmation before implementation.
```

---

## Shared animation helpers

### `triangleWave(progress)`

Used for breathing and pulsing:

```text
0 → 1 → 0 over one cycle
```

Used by:

```text
Z2 ambient
Z3 ambient
Z4 ambient
legs ambient
Z8 ambient
```

### `peakRampBrightness(...)`

Used for peristaltic strip waves:

```text
peak brightness
→ hold
→ ramp down to base brightness
→ return inactive when outside the wave envelope
```

Used by:

```text
Z1 active
Z2 active
```

These should later be ported as pure C++ helper functions.

---

# Zone behaviour

## Z1 — Mouth

File:

```text
src/zones/z1-mouth.js
```

Map:

```text
Z1 = 208 px
4 tapered strips:
33 px
50 px
60 px
65 px
```

Ambient behaviour:

```text
Sequential white pulse across the 4 strips.
Each strip starts after the previous one.
Pulse ramps up, holds, ramps down, then pauses.
```

Active behaviour:

```text
White peristaltic strip wave.
Strips fire in sequence.
Uses peakRampBrightness().
```

Main functions:

```text
z1_advanceAmbient(delta)
z1_renderAmbient(localIdx)
z1_advanceActive(delta)
z1_renderActive(localIdx)
```

---

## Z2 — Shoulder

File:

```text
src/zones/z2-shoulder.js
```

Map:

```text
Z2 = 325 px
6 tapered strips:
44 px
48 px
52 px
56 px
60 px
65 px
```

Ambient behaviour:

```text
Whole-zone cyan breathing pulse.
Uses triangleWave().
```

Active behaviour:

```text
Cyan peristaltic strip wave.
Runs top-to-bottom across 6 strips.
Supports 2 overlapping active cycles.
Uses peakRampBrightness().
```

Main functions:

```text
z2_advanceAmbient(delta)
z2_renderAmbient(localIdx)
z2_advanceActive(delta)
z2_renderActive(localIdx)
z2act_cycleBrightness(...)
```

---

## Z3 — Mid-body

File:

```text
src/zones/z3-midbody.js
```

Map:

```text
Z3 = 400 px
8 rings × 50 px
```

Ambient behaviour:

```text
Whole-zone blue breathing pulse.
Uses triangleWave().
```

Active behaviour:

```text
Center-out ring-pair strobe.
```

Ring-pair order:

```text
Step 0: rings 3 + 4
Step 1: rings 2 + 5
Step 2: rings 1 + 6
Step 3: rings 0 + 7
```

Active detail:

```text
Each ring pair strobes ON/OFF.
Brightness decreases across strobe cycles.
Between waves, the zone sits at active ambient brightness.
```

Main functions:

```text
z3_advanceAmbient(delta)
z3_renderAmbient(localIdx)
z3_advanceActive(delta)
z3_renderActive(localIdx)
```

---

## Z4 — Rear body

File:

```text
src/zones/z4-rear.js
```

Map:

```text
Z4 = 300 px
12 strips × 25 px
```

Ambient behaviour:

```text
Whole-zone purple breathing pulse.
Uses triangleWave().
```

Active behaviour:

```text
Center-out falling gradient.
Starts from the two centre strips and moves outward.
A bright falling peak travels along each strip with a trailing gradient.
Supports 2 overlapping active cycles.
```

Strip-pair order:

```text
Step 0: strips 5 + 6
Step 1: strips 4 + 7
Step 2: strips 3 + 8
Step 3: strips 2 + 9
Step 4: strips 1 + 10
Step 5: strips 0 + 11
```

Main functions:

```text
z4_advanceAmbient(delta)
z4_renderAmbient(localIdx)
z4_advanceActive(delta)
z4_renderActive(localIdx)
z4act_cycleBrightness(...)
```

---

## Z5 / Z6 — Legs

File:

```text
src/zones/legs.js
```

Map:

```text
Z5 + Z6 = one shared legs system
8 logical legs × 75 px = 600 px total

Z5 = legs 0–3 = 300 px
Z6 = legs 4–7 = 300 px
```

Important:

```text
Z5 and Z6 are not independent animation zones.
They are two physical outputs containing one shared 8-leg animation.
```

Ambient behaviour:

```text
Alternating leg pulse walk.
Odd legs pulse first.
Even legs pulse half a cycle later.
Amber/orange colour.
Uses triangleWave().
```

Active behaviour:

```text
Fire-zap / p-wave along each leg.
Odd legs start first.
Even legs start after groupDelayMs.
A bright gradient head travels along each 75-LED leg.
Colour shifts from base orange toward brighter yellow/orange around the zap.
```

Main functions:

```text
legs_advanceAmbient(delta)
legs_renderAmbient(localIdx, zoneOffset)
legs_advanceActive(delta)
legs_renderActive(localIdx, zoneOffset)
```

Porting rule:

```text
Port Z5/Z6 as one shared 8-leg animation split across two physical outputs.
Do not implement them as two unrelated zones.
```

---

## Z7 — Digestive tract

File:

```text
src/zones/z7-digestive.js
```

Map:

```text
Z7 = 75 px in PixelBlaze index space
7 physical strands wired in parallel
1 shared data output
```

Ambient behaviour:

```text
Slow red falling gradient pulse.
Baseline red glow remains between pulses.
The zone is never fully dark.
```

Active behaviour:

```text
Faster, brighter red falling peristalsis while button held.
Same pulse shape as ambient, but faster and brighter.
```

Important wiring detail:

```text
The desired visual motion is top → bottom.
The physical wiring runs bottom → top.
The code reverses local index before rendering.
```

Main functions:

```text
z7_advanceAmbient(delta)
z7_renderAmbient(localIdx)
z7_advanceActive(delta)
z7_renderActive(localIdx)
z7_pulseBrightness(...)
```

Porting rule:

```text
Preserve the reversed visual index unless the physical wiring changes.
```

---

## Z8 — Button-station string lights

File:

```text
src/zones/z8-string-lights.js
```

Map:

```text
Z8 = 100 px total
7 station strings × 14 px = 98 px
+ 2 spare pixels
OE Ch0
Start index 1908
```

Station mapping:

```text
station = floor(localIdx / 14)

If station >= 7:
- clamp to station 6
```

So the 2 spare pixels are treated as part of the final station.

Ambient behaviour:

```text
Gentle warm amber breathing.
Each station is phase-staggered so the station ring shimmers.
Uses triangleWave().
```

Active behaviour:

```text
Per-station bright chase.
If that station's matching button/zone trigger is active, that 14-pixel string chases from button station toward sculpture.
Other station strings remain ambient.
```

Important:

```text
Z8 does not have one global active state.
Each of the 7 strings follows its matching LED zone-active state.
```

In PixelBlaze that was `zoneActive[0..6]`.

In the ESP32 port, it should follow the new trigger-window active state:

```text
z8 station active = now < ledActiveUntil[station]
```

Main functions:

```text
z8_advance(delta)
z8_stationFor(localIdx)
z8_renderAmbient(station, posInString)
z8_renderActive(station, posInString)
```

---

# Full behaviour table

```text
Z1 / Mouth / 208 px / OE Ch1
Ambient: 4-strip sequential white pulse
Active: 4-strip white peristaltic wave

Z2 / Shoulder / 325 px / OE Ch2
Ambient: whole-zone cyan breathing
Active: 6-strip cyan peristaltic wave, two overlapping cycles

Z3 / Mid-body / 400 px / OE Ch3
Ambient: whole-zone blue breathing
Active: center-out ring-pair strobe wave

Z4 / Rear body / 300 px / OE Ch4
Ambient: whole-zone purple breathing
Active: center-out strip-pair falling gradient

Z5 / Front legs / 300 px / OE Ch5
Z6 / Back legs / 300 px / OE Ch6
Ambient: shared 8-leg alternating pulse
Active: shared 8-leg fire-zap / p-wave
Important: Z5 and Z6 are one shared leg animation split over two outputs

Z7 / Digestive / 75 px / OE Ch7
Ambient: slow red falling gradient pulse
Active: faster/brighter red falling peristalsis
Important: 7 physical strands are wired in parallel and share one data line

Z8 / Button-station strings / 100 px / OE Ch0
Ambient: warm phase-staggered breathing per station
Active: per-station chase based on trigger-window active state
Important: 7 × 14 px + 2 spare pixels clamped to station 6
```

---

# Pressure-test behaviour

`src/main-pressure.js` is an LED burn-in / pressure-test entry point.

It forces all LED zones active continuously:

```text
Z1 active
Z2 active
Z3 active
Z4 active
legs active
Z7 active
Z8 active
```

It does not use:

```text
buttons
zone-state machine
fire / poofers
```

This is useful as a concept for a future ESP32 LED test mode, but it is not the normal interactive behaviour.

---

# PixelBlaze fire / payoff notes

`src/fire/config.js` and `src/fire/poofers.js` are old PixelBlaze fire logic.

They confirm:

```text
allSevenHeld is used for old PixelBlaze fire payoff.
It is not a normal LED visual mode.
```

Old PixelBlaze payoff:

```text
All 7 buttons held:
- Z1 mouth fire becomes continuous
- Z5/Z6 leg poofers perform coordinated dance
```

Do not port this into ESP32 FIRE logic.

Important polarity mismatch:

```text
Old PixelBlaze poofers were driven HIGH to fire.

Current ESP32 FIRE outputs are active-LOW:
HIGH = off
LOW = fire
```

The ESP32 baseline is the one to keep.

---

# What should eventually be ported to ESP32

Port the LED behaviour and mapping concepts:

```text
Zone pixel map
Output channel map
Zone-local index mapping
Z1/Z2 tapered strip maps
Z3 ring map
Z4 strip map
Z5/Z6 shared 8-leg map
Z7 reversed visual index
Z8 station-string map and spare-pixel handling
Ambient animation timing constants
Active animation timing constants
triangleWave()
peakRampBrightness()
Frame advance / render structure
Trigger-window LED active-state model
Pressure-test / all-active LED test concept
```

Possible future ESP32/C++ structure:

```text
LedLayout
LedConfig
LedState
LedEngine
Zone render modules
Output Expander / LED output packing
LED test modes
```

Do not implement yet.

---

# What should not be ported

Do not port:

```text
PixelBlaze GPIO numbers
PixelBlaze pinMode/digitalRead code directly
PixelBlaze digitalWrite fire logic
PixelBlaze poofer pin map
PixelBlaze fire choreography
PixelBlaze fire polarity
PixelBlaze held-button/latch active-state model
PixelBlaze all-7 fire payoff into ESP32 FIRE logic
PixelBlaze UI slider handler directly
PixelBlaze generated dist files
PixelBlaze tools
archive/backups/old pattern files
```

Do not change the already-tested ESP32 controller baseline.

---

# Known hardware / source-context notes

The docs mention post-SOAK hardware issues:

```text
Z2 stopped working at the event.
Z5/Z6 stopped working at the event.
Z3 animation had previously been wrong.
Z7 was never fully installed and had wiring-direction problems.
Brightness had been conservatively low.
```

Implication:

```text
Do not assume every old issue was a software issue.
Some failures were likely physical wiring, connector, or installation issues.
```

---

# Open questions for Whit / California team

## 1. Button 8 LED role

```text
PixelBlaze only has 7 LED buttons.
ESP32 controller has 8 tested buttons.
Confirm whether Button 8 should:
- affect LEDs,
- only participate in FIRE9 / Big Poof,
- or trigger a new LED effect.
```

## 2. Big Poof visual behaviour

```text
ESP32 Big Poof is Button 1 + Button 8.
PixelBlaze payoff was all 7 buttons.
Confirm whether the ESP32 LED engine should:
- ignore Big Poof visually,
- add a new Big Poof LED visual,
- or preserve only the original 7-zone LED behaviour.
```

## 3. Exact ESP32 button-to-LED mapping

```text
PixelBlaze maps 7 buttons to 7 LED zones.
ESP32 has 8 buttons and 9 FIRE outputs.

Confirm the intended LED mapping, especially:
- Button 5 / Button 6 relationship to shared Z5/Z6 leg animation
- whether individual leg FIRE buttons should extend the same shared leg LED window
- whether Button 8 extends any LED window
```

## 4. Z8 spare pixels

```text
Z8 is 100 px.
7 station strings × 14 px = 98 px.
The final 2 pixels are clamped to station 6 in code.
Confirm whether these are real spare LEDs, physical padding, or just safe fallback.
```

## 5. Z7 physical wiring

```text
Docs say Z7 is 7 strands wired in parallel sharing one data line.
Confirm current Burning Man wiring will still be parallel 75 px, not 7 separately addressable 75 px runs.
```

## 6. Output channel order

```text
Confirm the current physical output order remains:
Ch0 = Z8
Ch1 = Z1
Ch2 = Z2
Ch3 = Z3
Ch4 = Z4
Ch5 = Z5
Ch6 = Z6
Ch7 = Z7
```

## 7. LED chipset / output handling

```text
Confirm whether the ESP32 output plan needs separate handling for:
- WS2812B zones
- WS2811 zones
- GRB colour order
- different physical output lengths
```

---

# Bottom line

The PixelBlaze LED behaviour is now well understood.

The current PixelBlaze LED system is:

```text
7 interactive biological LED zones
+ 1 station-string zone
+ ambient state for every zone
+ active state per PixelBlaze button/zone
+ Z8 mirroring the 7 station active states
+ old all-7 fire payoff, not a normal LED mode
```

The ESP32 version should preserve the LED mapping, timing, and visual behaviour, but replace the active-state model:

```text
Do not use:
zoneActive = buttonHeld && !latched

Use:
zoneActive = now < ledActiveUntil[zone]
```

That means:

```text
accepted button/fire trigger → active LED animation runs for 10 seconds
accepted repeat trigger → active LED timer extends to 10 seconds from the new trigger
no repeat trigger → active LED animation returns to ambient after 10 seconds
```

For ESP32, port the LED behaviour.

Do not port the old PixelBlaze controller assumptions, GPIOs, held-button latches, fire polarity, or poofer logic.
