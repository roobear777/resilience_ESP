# Power and Ground Design

Status: **proposal / missing subsystem.** Every other document in this repo describes
signal — pins, protocols, logic levels, channel maps. None describes power. The
Output Expander failure on 2026-07-29 is attributed to LED current being routed
through the expander board, which is a power-distribution problem, not a signal
problem.

This document is the missing half.

## Rule Zero

> **No LED current passes through any PCB.**

Not the Output Expander, not the ESP32, not the buffer board, not a breadboard
rail. Controller boards carry *signal and their own logic supply only*.

LED power comes off a distribution bus and goes directly to the strips. This is
the single rule that, had it existed, would have saved the expander. It matters
more now, because the ESP32-S3 is both the LED controller and the fire
controller — losing it the same way loses fire control with it.

## Current Budget

### Method

WS2812B draws roughly **20 mA per colour channel** at full output. A pixel showing
a single-hue colour lights one or two channels; white lights all three.

```
zone current = pixels x (20 mA x active channels) x brightness
```

### Per-zone worst case

Using the zone colours and peak brightness values from the animation config.
**Physical** LED counts, not logical pixel counts.

| Zone | Physical LEDs | Colour | Ch | Peak | Peak current |
|---|---:|---|---:|---:|---:|
| Z1 mouth | 208 | white | 3 | 0.90 | 11.2 A |
| Z2 shoulder | 325 | cyan | 2 | 0.95 | 12.4 A |
| Z3 midbody | 400 | blue | 1 | 0.85 | 6.8 A |
| Z4 rear | 300 | purple | 2 | 1.00 | 12.0 A |
| Z5 front legs | 300 | orange/yellow | 2 | 0.90 | 10.8 A |
| Z6 back legs | 300 | orange/yellow | 2 | 0.90 | 10.8 A |
| Z7 digestive | **525** | red | 1 | 1.00 | 10.5 A |
| **Total** | **2,358** | | | | **74.5 A** |

### Two things this table exposes

**1. Z7 is 525 physical LEDs, not 75.**

The firmware treats Z7 as 75 logical pixels because seven strands run in parallel
off one data line. The *wire* sees all seven strands. Any power budget built from
`LED_TOTAL_PIXEL_COUNT` under-counts the sculpture by 450 LEDs.

```
logical pixels  = 1,908   <- what the code says
physical LEDs   = 2,358   <- what the power supply sees
```

**2. The sculpture at idle is already near the supply limit.**

Estimated ambient draw with the documented base brightnesses:

| Zone | Ambient brightness | Ambient current |
|---|---:|---:|
| Z1 | 0.04 | 0.5 A |
| Z2 | 0.04-0.14 | 1.2 A |
| Z3 | 0.04-0.12 | 0.6 A |
| Z4 | 0.03-0.10 | 0.8 A |
| Z5+Z6 | 0.15-0.35 | 4.8 A |
| Z7 | 0.25-1.00 (never dark) | 3.2 A |
| **Total** | | **~11 A** |

Against a **15 A** buck converter, that is ~73% of capacity with nobody touching
the sculpture. A single button press drives one zone to peak and can add another
10-12 A, which exceeds the supply.

Legs and Z7 dominate ambient because their base brightnesses are an order of
magnitude higher than the other zones (0.15 and 0.25, versus 0.03-0.04).

> These figures scale with the saved master brightness in `led_settings`. If the
> deployed master is well below 1.0, divide accordingly. The *ratios* between
> zones hold regardless, and Z7's 7x multiplier holds regardless.

### Supply sizing

| Scenario | Draw |
|---|---:|
| Ambient only | ~11 A |
| Ambient + one zone active | ~20 A |
| All zones active (payoff) | ~50-75 A |
| `pressure-test` at brightness 1.0 | >75 A |

**Recommendation:** either size for ~40 A with headroom, or split into two or
three independent 5 V supplies by zone group, each with its own bus. Splitting is
usually cheaper and fails more gracefully — one supply dropping takes out one
group, not the sculpture.

**Note on the diagnostic guide:** `hardware-diagnostic-guide.md` instructs
running `pressure-test` at `globalBrightness = 1.0` for 30+ minutes. On a 15 A
supply that test will brown out the rail. Brownout while data is being clocked is
its own hardware-loss mechanism. Either resize the supply first or cap the test
brightness — do not run that procedure as written.

## Distribution Topology

### Target

```
        12 V battery
             |
        [main fuse]
             |
       [rocker switch]
             |
      +------+------+
      |             |
  [buck 5 V]    [inverter]
      |
  [main 5 V fuse]
      |
  === +5 V BUS BAR ===================
   |    |    |    |    |    |    |
  [F]  [F]  [F]  [F]  [F]  [F]  [F]     <- per-zone blade fuses
   |    |    |    |    |    |    |
   Z1   Z2   Z3   Z4   Z5   Z6   Z7     <- direct to strips

  === GND BUS BAR ====================
   |    |    |    |    |    |    |   |
   Z1   Z2   Z3   Z4   Z5   Z6   Z7   +--> single bond to logic ground
```

### Rules

- **Bus bars, not daisy chains.** Every zone gets its own pair of conductors back
  to the bars. No zone's current flows through another zone's wiring.
- **Fuse the +5 V side per zone.** Automotive blade fuses are cheap, rated, and
  replaceable in the dark. Size at ~1.5x the zone's expected peak, and always
  below the wire's ampacity.
- **The buffer board taps the same 5 V bus** for its VCC. It draws milliamps. Its
  ground bonds to the ground bar at the same single point as the ESP32.
- **Ground is a star, not a mesh.** One bond point between LED power ground and
  logic ground. Not several.

### Why the star matters

"All grounds are connected" is necessary but not sufficient. If LED return
current shares a conductor with signal ground, the strip's local ground sits
above the ESP32's ground by I x R.

At 20 A through a few feet of undersized wire that is several hundred millivolts.
The strip then compares `(signal - offset)` against its 3.5 V threshold — so
ground offset eats directly into the logic margin the AHCT244 buffer exists to
restore.

Sharing a ground is not the same as being at the same potential.

## Voltage Drop and Injection

WS2812B is specified 3.5-5.3 V, but below roughly **4.2 V at the pixel** you get
visible colour shift before you get failure. Blue dies first — blue LED forward
voltage is ~3.2 V against red's ~2.0 V, so as the rail sags, blue fades and
everything drifts warm.

**If someone reports colour shifting down the length of a run, that is a voltage
symptom, not a code symptom.**

### Wire resistance (round trip — count both conductors)

| AWG | mOhm/m | mOhm/m round trip |
|---:|---:|---:|
| 18 | 21 | 42 |
| 16 | 13 | 26 |
| 14 | 8.3 | 17 |
| 12 | 5.2 | 10 |

```
V_drop = I x R_roundtrip x length
```

### Injection points

Inject 5 V and ground **every 100-150 pixels**, and at **both ends** of any run
longer than that.

| Zone | Physical LEDs | Injection |
|---|---:|---|
| Z1 | 208 | both ends |
| Z2 | 325 | both ends + midpoint |
| Z3 | 400 | both ends + 2 midpoints |
| Z4 | 300 | both ends + midpoint |
| Z5/Z6 | 4 x 75 per side | feed each leg independently |
| Z7 | 7 x 75 strands | feed each strand independently |

Z5/Z6 already fail this way. `hardware-diagnostic-guide.md` notes multiple legs
dying together and identifies the likely cause as "the power distribution bus."
Feeding each leg from the bar independently fixes that class of failure
permanently.

## Signal Wiring

- **Data and its ground travel together**, in the same cable, ideally twisted.
  CAT5/CAT6 is close to the 100 ohm the series resistors are matched for.
- **Never route data alongside the 120 VAC solenoid harness.** Cross at right
  angles if they must meet. Separate glands where possible.
- **Series 100 ohm at the buffer end** of every data line — see
  [`buffer_board.md`](buffer_board.md).

## Verification

After building, before trusting it:

1. Meter continuity from each zone's connector back to the bus bar.
2. Confirm there is **exactly one** bond between power ground and logic ground.
3. Under ambient load, measure 5 V at the far end of every run. Above 4.5 V.
4. Under a single zone at peak, measure again. Still above 4.5 V.
5. Clamp-meter the main 5 V feed at ambient and with one zone active. Compare
   against the table above. A large discrepancy means the model is wrong, and
   you want to know that before the event, not during it.

See [`preflight_checklist.md`](preflight_checklist.md) for the full sequence.
