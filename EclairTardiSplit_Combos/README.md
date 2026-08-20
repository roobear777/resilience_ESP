# Eclair / Tardi Split — Combo Lighting

Experimental build. **`../EclairTardiSplit/` remains the baseline** — if this
doesn't work out, flash that.

Only `Eclair_LED` differs. `Tardi_Fire` and `LCD_Driver_Test` are byte-identical
copies so this folder is a complete flashable set.

---

## What this adds

Two independent axes, driven entirely by the buttons that already exist:

| Axis | Control | Effect |
|---|---|---|
| **Mood** | Button 8 toggles | what *kind* of creature it is |
| **Convergence** | how many buttons are held | how *unified* it is |

### Colour code — count picks the colour, the held zones get it

The buttons are at seven separate stations, so **any combination requires more
than one person**.

**Which zones change:** only the ones whose buttons are held. Press 2 and 3 and
Z2 and Z3 turn green *together*; every other zone carries on undisturbed. You
see your own zone respond, and you see it match the other person's.

**Which colour:** how many zones are lit — *not* how many buttons are down.

| Lit zones | ORGANIC | CHARGED |
|---|---|---|
| 1 | zone's own colour | zone's own colour |
| 2 | red | green |
| 3 | green | red |
| 4 | magenta | cyan |
| 5 | yellow | magenta |
| 6 | cyan | yellow |
| 7 | orange | blue |
| 8 (full body) | blue | orange |

Full snap, not a blend — halfway convergence isn't readable at distance, and
recolouring the *whole* sculpture drowned out the connection between what you
pressed and what changed.

> **Why these colours, in this order.** The first table walked green → cyan →
> blue → violet and gave 3 and 4 the same hue, 5 and 6 the same, 7 and 8 the
> same. Two failures at once: a fourth person could join and nothing visibly
> changed, and the steps that *did* change were 0.07–0.08 apart along one side
> of the wheel, where hue discrimination is worst. Everything past three
> buttons looked like the same blue. **That is why four onward was confusing.**
>
> The table is now the result of a brute-force search over orderings of seven
> LED-legible hues:
>
> | | old | new |
> |---|---:|---:|
> | min gap between adjacent counts | 0.07 | **0.30** |
> | min gap between the two moods | collisions | **0.30** |
>
> Deliberately *not* a temperature ramp — HSV compresses red→yellow into
> 0.00–0.15, so any ramp has tiny gaps at the warm end. Legibility at thirty
> feet in the dark beats narrative.

> **Colour follows lit zones, not button count.** Button 8 has no zone. Driving
> the colour off the raw button count meant holding B8 alongside three zone
> buttons showed the *four*-zone colour while only three zones were lit — and
> releasing B8 changed the colour without changing anything you could see.

> **The colour eases between steps.** A group arriving one at a time used to
> make the hue jump on every press — four different colours in under a second,
> which is most of what made a crowd feel chaotic. It now slides round the
> wheel, so a growing group reads as one continuous change.

Nobody has to be told the rules. With 8 buttons there are 28 possible pairs;
mapping those to specific effects would mean most triggers are accidental and
nobody connects cause to effect.

The **B1+B8 full-body payoff** is the one deliberate exception: every zone
active and every zone taking the colour.

### Mood — Button 8

B8 has no body zone (7 zones, 8 buttons) and no station string (only 7 exist).
So instead of giving it a pattern, it toggles the whole sculpture:

| | **ORGANIC** | **CHARGED** |
|---|---|---|
| Hue shift (all zones, incl. ambient) | none | +0.50 around the wheel |
| Signature flash colour | cyan | red |
| Combo colour set | cool | warm |
| Length | **×1.00** | ×0.60 — short, tight |
| Speed | **100%** | 160% |
| Reads as | the sculpture as built | agitated, electric, awake |

**Two moods, not three, so it's a toggle rather than a cycle** — every press
visibly flips something and you always know which state you're in.

Toggling also fires a **700 ms full-sculpture flash** in the new mood's
signature colour, so a B8 press is unmistakable rather than something you have
to go looking for. Between that, the ambient hue shift, and the mood swapping
the entire combo colour set, B8 has three visible consequences.

> **ORGANIC is an exact no-op, on purpose.** At boot, and any time nobody is
> pressing anything, `ledComboApply()` returns the colour untouched via a fast
> path — not "shifted by zero", untouched. The resting sculpture is identical
> to the baseline build.
>
> An earlier version had ORGANIC at `lengthScale 1.35`, reasoning that it
> should be the "long, flowing" mood. That was wrong. Z8's chase tail is 5 px
> on a **14 px** station string; ×1.35 stretched it to 6.75 — nearly half the
> run — turning a crisp chase into a smear, by default, before anyone had
> touched a button. The default look is not a design opportunity.

---

## Why the mood changes both speed *and* length

The zones are shaped two different ways. Either knob alone would leave half the
sculpture unmoved:

**Time-shaped** — character comes from timing, responds to **speed**
: Z1 mouth (peristaltic), Z2 shoulder (peristaltic), Z3 midbody (strobe)

**Space-shaped** — character comes from how many pixels are lit at once,
responds to **length**
: Z4 rear (`gradientWidth`), Z5/Z6 legs (`zapLength`), Z7 digestive
(`gradientLength`), Z8 stations (`tailLength`)

---

## The structural change

The old B2+B6 green override returned **before** any zone code ran:

```cpp
if (ledAllGreenOverride) {
  return { 0.333f, 1.0f, 1.0f };   // solid green, animations frozen
}
```

That stopped the sculpture breathing and made it look broken rather than
transformed. Replaced with a **post-process stage** at the end of
`ledApplySettingsToColor`, after the zone has rendered:

```cpp
tuned = ledComboApply(tuned, active, comboZone);
```

The animation keeps running underneath. Only **hue and saturation** move.

### Brightness is never touched

Ambient draw already sits near the supply ceiling. "Everything gets brighter"
isn't available to us — and concentrating and unifying colour reads as more
dramatic than adding light anyway.

`lengthScale` is the one knob that costs current, since more consecutive lit
pixels is literally more amps. That's why **CHARGED is the short mood**, so the
two roughly balance rather than one being a step up in draw.

---

## Two details that matter

**Shortest-arc hue interpolation.** Hue is a circle. Interpolating 0.9 → 0.1 by
simple lerp travels backwards through green and cyan — 0.8 of the wheel instead
of 0.2. `ledComboLerpHue()` wraps the short way.

**Saturation goes to full.** Z1 renders as white (saturation 0), and hue is
meaningless on a white pixel — without this the mouth would sit unchanged while
the zones beside it changed colour. Driving to *full* rather than partway also
matters for legibility: ambient zones are dim (0.03–0.25), and at that
brightness a pastel colour is barely a colour. Saturation is the only lever
available, since brightness is off limits.

**Buttons are filtered asymmetrically.** A pin must read HIGH *continuously*
for `TRIGGER_HOLD_MS` (150 ms) to count as a press; any low sample resets the
run. Release needs only 50 ms, so nothing feels sticky.

The earlier symmetric 30 ms debounce was far too permissive — an unwired input
next to eight lines switching at 800 kHz holds high for well over 30 ms at a
time, and every one of those was accepted as a press. That is why zones went
active on their own. **The real fix is still the hardware RC network**; this is
mitigation. Run `buttons` to see which pins are dirty.

---

## Files

| File | Status |
|---|---|
| `led_combo.h` / `.cpp` | **new** — mood table, convergence ladder, hue maths |
| `led_engine.cpp` | green override removed; combo stage added; speed scale |
| `led_engine.h` | `ledEngineSetAllGreenOverride` removed |
| `led_z4_rear.cpp`, `led_legs.cpp`, `led_z7_digestive.cpp`, `led_z8_stations.cpp` | active length constants now scaled by mood |
| `Eclair_LED.ino` | asymmetric button filter, held-zone mask, B8 mood toggle, `buttons` noise check |

Everything else is unchanged from the baseline.

---

## Tuning

Everything lives in one table at the top of `led_combo.cpp`:

```cpp
static const LedMoodConfig LED_MOODS[LED_MOOD_COUNT] = {
  //  name        hueShift  signature  lengthScale  speedPercent
  { "ORGANIC",      0.00f,     0.50f,      1.00f,        100 },  // keep at identity
  { "CHARGED",      0.50f,     0.02f,      0.60f,        160 },
};
```

`speedPercent` is an **integer**, not a float. The engine folds it into a
`uint64_t` time calculation, and a float there promotes the whole expression to
single precision — 24-bit mantissa — which quantises the time base once
`nowMs * speedPercent` passes 16.7 million, under three minutes of uptime. That
affected ambient too, since even a `1.0f` multiplier forces the float path.

And the colour code, indexed by how many buttons are held:

```cpp
static const float COMBO_HUE[LED_MOOD_COUNT][9] = {
  //         0      1      2      3      4      5      6      7      8
  /* ORG */ {0.00f, 0.00f, 0.33f, 0.50f, 0.50f, 0.67f, 0.67f, 0.78f, 0.78f},
  /* CHG */ {0.00f, 0.00f, 0.00f, 0.08f, 0.08f, 0.15f, 0.15f, 0.95f, 0.95f},
};
```

Button filtering lives in `Eclair_LED.ino`:

```cpp
const unsigned long TRIGGER_HOLD_MS = 150;   // continuous HIGH required
const unsigned long RELEASE_HOLD_MS = 50;
```

---

## Behaviour notes

**B8 also fires FIRE8.** Every mood change is punctuated by a poof — you can't
change the look quietly. That may be good or annoying; it's a Tardi-side
question, not an Eclair one.

**Mood only toggles on a clean B8 press with nothing else held.** B1+B8 is the
head poof / full-body combo, so an unconditional toggle would scramble the
palette every time the payoff runs.

**Mood speed and length affect active animations only.** The mood's *hue shift*
does apply to ambient, which is what makes B8 visible with nobody pressing.

**Convergence eases rather than snapping.** Releasing a button looks like the
sculpture relaxing instead of a light switch. Costs nothing.

---

## Serial

Everything from the baseline, plus:

```
mood            toggle ORGANIC / CHARGED
buttons         1s noise check on all 8 inputs
colours         the colour code table
```

`buttons` samples every input hard for a second and reports what fraction read
high. With nothing pressed, every pin should be 0%:

```
btn  gpio  high%  verdict
  1     4    0.0  clean
  5    15   34.2  FLOATING - phantom presses
```

The status banner now reports:

```
mood   : ORGANIC  hueShift 0.00  length x1.00  speed 100%
combo  : 2 held, 2 lit  hue 0.00->0.00  blend 1.00  zones[.23.....]  red
```

---

## Verification

`led_combo.cpp` is covered by a host-side test that links against the real
file and checks:

1. at rest in ORGANIC every pixel comes back bit-identical
2. every lit-zone count gives a distinct hue, ≥0.25 from its neighbours
3. Button 8 does not shift the colour
4. only masked zones are recoloured; unheld zones are untouched
5. hue easing converges and never leaves 0..1
6. full-body mask reaches 8 lit zones and full blend
7. brightness is never modified
8. a mood toggle re-picks the colour immediately

All pass. The rest of the sketch is **not** compiled or run on hardware — the
mood and colour values are chosen to be *legible*, and want tuning against the
real sculpture at night, which is the only place the numbers mean anything.

---

## Fire behaviour (Tardi)

```
button N            ->  FIRE N, 100 ms, repeating every 1 s while held

all 8 held          ->  t = 0.0 s   FIRE1..FIRE8 all on TOGETHER, FIRE9 on
                        t = 1.0 s   FIRE1..FIRE8 all off
                        t = 1.5 s   FIRE9 off, lockout begins
                        t = 31.5 s  lockout ends
```

**The zones fire in sync, driven from the poof's clock.** Normally each zone's
repeat timer runs from the moment *its own* button was pressed, so eight people
pressing at slightly different times leaves the zones scattered across the
second. For the payoff they have to land together, so during the poof the state
machine writes all eight outputs directly and the per-button pulse table is
bypassed. Measured: every zone starts and stops within the same 5 ms frame.

**The lockout covers all nine outputs.** While it is in effect no button does
anything — pressing button 3 will not fire zone 3.

Invariant: `BIG_POOF_ZONE_MS <= BIG_POOF_DURATION_MS`, so the zones finish
first and FIRE9 is alone for the tail of the poof.

The poof is a fixed-length **shot, not a hold**: releasing partway does not cut
it short, and holding cannot extend it. Re-arming needs the all-8 combo
released *after* the cooldown has finished, so every poof is a deliberate act.

### What this replaced

Three separate problems:

- **All-8 fired a 500 ms burst on every output**, and the sustained head poof
  was on **B1 + B8**. All-8 satisfied both conditions, so both ran at once —
  and both drove FIRE9. They interacted through the shared pulse table and
  dropped FIRE9 for a frame partway through. FIRE9 is now owned exclusively by
  one state machine, and `updateFirePulseStates()` deliberately loops
  `0..NUM_BUTTONS-1` so it can never touch it.
- **No cooldown existed at all.** Release and re-press re-fired immediately.
- **The boot interlock never worked.** It checked only
  `debouncedButtonState[]`, which initialises to `false` and takes
  `DEBOUNCE_MS` to catch up — so on the first loop iteration every button
  looked low, all eight were marked seen-low, and the board armed itself before
  debounce resolved. Holding buttons at power-up armed instantly and then
  fired: precisely what the interlock exists to prevent. It now requires both
  raw and debounced low.

B1 + B8 no longer has any special meaning.

### Verification

A host-side harness stubs Arduino, compiles the real `.ino`, and drives it with
virtual buttons and a virtual clock:

1. boot interlock blocks fire until every input has been seen low
2. one button drives its own zone, repeating, FIRE9 untouched
3. all-8 gives a 1.5 s poof — measured 1495 ms
3b. the poof itself: all 8 zones on for 1000 ms each, every one starting and
    stopping in the same frame; FIRE9 1495 ms
4. still held: no refire 10 s into the cooldown
5. release + re-press mid-cooldown: still locked out
6. **every** poofer is locked out during the cooldown, not just FIRE9
7. holding through the cooldown does not auto-fire; release + press does
8. continuous hold never re-arms
9. B1+B8 alone does nothing to FIRE9
10. 7 of 8 buttons is not enough
11. releasing mid-poof still completes the full 1.5 s
12. zone poofers resume once the lockout lifts
13. a single zone still works normally when no poof has happened

All pass. New serial command **`poof`** reports state and cooldown remaining.
