# Eclair / Tardi Split

Current firmware for the Resilience tardigrade. **Two ESP32-S3 boards, two
sketches, no link between them.**

| | `Tardi_Fire/` | `Eclair_LED/` |
|---|---|---|
| Job | buttons, FIRE1-9, OLED | buttons, Z1-Z8 animation, WiFi tuning |
| Buttons | 8 inputs | 8 inputs — **the same wires** |
| Outputs | 9 relay lines, active-LOW | 8 WS2812 lanes, LCD_CLOCKLESS |
| OLED | yes, GPIO1/2 | no |
| WiFi | **none** | TARDI-LED AP |
| Box | existing enclosure | new box, near the sculpture |

---

## How we got here

Worth reading before changing anything, because most of the decisions below
look arbitrary until you know what they are reacting to.

### 1. The sculpture ran on a PixelBlaze, then on our own ESP32

SOAK ran a PixelBlaze V3 with an Output Expander. After SOAK the animation
engine was rewritten as ESP32-S3 firmware, still driving the same Output
Expander over a 2 Mbps UART on GPIO39. That is the `esp32_code_july_24` build.

### 2. The expander died

Some wiring changes went in, and afterwards the expander stopped putting out
LED data. Best explanation, and the one the team reached independently: **LED
power was being routed through the expander board.** That board is a data
driver. The Pro version is the one built for power distribution.

Two things made this worse than it had to be:

- Nothing in the repo documented power. Every doc covered signal — pins,
  protocols, logic levels, channel maps. `led_output_expander.md` explicitly
  said power distribution was out of scope, and nothing else picked it up.
- The bring-up procedure at the time was "plug a strip into different outputs
  and see which light." Hot-plugging a strip is the classic way to destroy
  these drivers: if the data pin makes contact before ground, return current
  flows through the data pin.

A replacement was two weeks out, and after the person who knew the hardware
best had to leave.

### 3. Three plans

1. Drive the strips straight off ESP32 GPIO, through a logic buffer.
2. Add a second ESP32 to generate the WS2812 signals.
3. Split the lanes across two ESP32s, 3 and 4.

Plans 2 and 3 were built around a real constraint — **the ESP32-S3 has only
four RMT TX channels** — but that constraint only applies if you use RMT. The
S3's LCD_CAM peripheral drives 16 lanes in parallel off DMA. So the two-chip
designs were solving a problem that a different peripheral removes.

Plan 1 also had the better shape: the Output Expander did three jobs —
**routing, 5V level shifting, and 100 ohm series termination**. Routing moves
into firmware for free (each lane is a slice of one framebuffer). The other two
are an SN74AHCT244 and eight resistors. That is not adding a part; it is
keeping the half of the expander that software can't replace.

### 4. Proving it

The first driver test appeared to show the 4-channel limit. It didn't — it had
`#define FASTLED_ESP32S3_LCD_DRIVER`, which is not a real macro (no `S3`), and
a `#define` in a `.ino` never reaches FastLED anyway because the IDE compiles
libraries separately. The test ran on RMT and hit the exact limit it was trying
to escape.

Fixed via `build_opt.h` and the explicit Channel API. Result:

```
driver : 7/7 on LCD_CLOCKLESS   ->  PASS, all lanes parallel
show   : 22.6 ms  =  ~12.0 ms on the wire + ~10.6 ms buffer prep
rate   : 43.5 fps
```

That also settled the one genuine risk: **the DMA buffer allocates at 400 px on
the longest lane.** `LCD_Driver_Test/` is that sketch, kept as a bring-up tool.

### 5. Why it became two boards anyway

Not for signal reasons — the enclosure is simply full, so the LED driver had to
move to a second box regardless.

Once a second box was happening, the question became what goes in it. An early
proposal had the two boards talking over **I2C**. That would have been worse
than the problem: I2C is open-drain, bidirectional, and specified for about
400 pF of bus capacitance — a few feet. Over 20+ feet past switching solenoids
it fails intermittently rather than cleanly.

The idea that made it work: **run the button wires to both boards.** Then there
is no link at all. Both boards see the same edge at the same instant. The
buttons *are* the synchronisation mechanism.

---

## Why split by function, not by zone

**No link to fail.** Covered above. If the "link" breaks, a button has broken,
and a multimeter finds that in ten seconds.

**All zones stay on one chip.** The animations sweep *across* zones — the
peristaltic wave, the full-body payoff. Splitting Z1-Z3 onto one board and
Z4-Z7 onto another (Plan 3) would have needed permanent phase-locking over a
link. Splitting by function doesn't.

**Fire gets a dedicated controller.** Previously one chip ran WiFi, an HTTP
server, an OLED, ~2,000 pixels of DMA and nine relays. The LED write blocked
the loop for tens of milliseconds — which is exactly how long a solenoid stays
open past when it should have closed. Tardi is now a few hundred lines with no
WiFi, no DMA, no frame loop.

**Z8 came back.** The single-board direct-drive build had to drop the
button-station strings: GPIO1/2 were the OLED, and there was no eighth lane.
With the OLED on Tardi, GPIO38 freed up. Back to 2,008 pixels across 8 zones.

---

## Folder contents

```
EclairTardiSplit/
├── Eclair_LED/         LED board — sketch + the zone engine modules
├── Tardi_Fire/         fire board — single self-contained sketch
├── LCD_Driver_Test/    bring-up tool, run this first on new hardware
└── docs/
    ├── two_board_split.md    architecture reference
    ├── buffer_board.md       SN74AHCT244 build sheet
    ├── power_and_ground.md   the missing subsystem
    ├── preflight_checklist.md staged bring-up
    └── enclosure_review.md   assessment of the current build
```

---

## Pin maps

### Tardi (fire)

| Function | GPIO |
|---|---|
| Buttons 1-8 | 4, 5, 6, 7, 15, 16, 17, 18 |
| FIRE1-8 | 8, 9, 10, 11, 12, 13, 14, 21 |
| FIRE9 / head poof | 47 |
| OLED SDA / SCL | 1, 2 |

UART0 (GPIO43/44) is free — one of the pins the split gave back.

### Eclair (LED)

| Function | GPIO | Pixels |
|---|---|---|
| Buttons 1-8 | 4, 5, 6, 7, 15, 16, 17, 18 | |
| Z1 mouth | 1 | 208 |
| Z2 shoulder | 2 | 325 |
| Z3 midbody | 39 | 400 |
| Z4 rear | 40 | 300 |
| Z5 front legs | 41 | 300 |
| Z6 back legs | 42 | 300 |
| Z7 digestive | 43 | 75 |
| Z8 stations | 38 | 100 |
| LCD driver padding | 0 | **unwired** |

Free after all that: GPIO3, GPIO44.

> **Button pins are identical on both boards on purpose.** One physical wire
> per button reaches both boards on the identically-numbered pin. Keep this
> invariant; it is what makes the split debuggable.

---

## Shared button wiring

One wire per button feeding two boards. ESP32 inputs are high-impedance CMOS
(nanoamp leakage), so two in parallel is nothing.

```
        3.3V (single source, ideally its own regulator)
             |
          [button]
             |
   +---------+---------+
   |                   |
  1k                  1k          <- series, at each board
   |                   |
   +--100nF--GND       +--100nF--GND    <- RC filter, at each board
   |                   |
 Tardi GPIO        Eclair GPIO
             |
           10k                     <- ONE pulldown, at the button end
             |
            GND
```

- **One 10k pulldown** at the button end, not one per board.
- **1k series at each GPIO** — limits backfeed if one board is powered and the
  other isn't. ESP32 pins have protection diodes to VDD, so driving a pin on an
  unpowered chip can partially power it through them.
- **100nF at each input** — 1k x 100nF = 100 us. Kills RF pickup from the
  solenoid harness; invisible to a real press and to the software filter.
- **Common ground between the boxes** on a real conductor.
- **Twisted pair** per button, signal with its own ground.

Both sketches count rejected glitches and report them. A climbing glitch count
on the fire board is worth acting on — there the failure mode is unintended
fire.

---

## Behaviour

| Input | Tardi | Eclair |
|---|---|---|
| Button 1-7 | FIRE1-7, 100 ms, repeating every 1 s while held | triggers Z1-Z7 |
| Button 8 | FIRE8 | no zone — toggles the **mood** |
| **B1 + B8** | nothing special | full-body: all zones |
| **2+ held** | — | the held zones snap to one shared colour |
| **All 8** | the big poof, then a 30 s lockout on every poofer | full body |

A press **restarts** a zone's window rather than being ignored while it is
already active, so a press always gets a response.

Buttons are filtered **asymmetrically** on Eclair: a pin must read HIGH
*continuously* for `TRIGGER_HOLD_MS` (150 ms) to count as a press, and any low
sample resets the run. Release needs only 50 ms, so nothing feels sticky. The
earlier symmetric 30 ms debounce was far too permissive — an unwired input next
to eight lines switching at 800 kHz holds high for well over 30 ms at a time,
and every one of those was accepted. That is why zones went active on their
own. **The real fix is still the RC network above**; this is mitigation. Run
`buttons` to see which pins are dirty.

---

## Combo lighting (Eclair)

Two independent axes, driven entirely by the buttons that already exist:

| Axis | Control | Effect |
|---|---|---|
| **Mood** | Button 8 toggles | what *kind* of creature it is |
| **Convergence** | how many buttons are held | how *unified* it is |

Nobody has to be told the rules. With 8 buttons there are 28 possible pairs;
mapping those to specific effects would mean most triggers are accidental and
nobody connects cause to effect. Driving off *how many* rather than *which*
means the sculpture visibly rewards more people joining in, and accidental
pairs — inevitable with seven strangers — still look intentional.

### Colour code — count picks the colour, the held zones get it

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
> to the pre-combo build.
>
> An earlier version had ORGANIC at `lengthScale 1.35`, reasoning that it
> should be the "long, flowing" mood. That was wrong. Z8's chase tail is 5 px
> on a **14 px** station string; ×1.35 stretched it to 6.75 — nearly half the
> run — turning a crisp chase into a smear, by default, before anyone had
> touched a button. The default look is not a design opportunity.

### Why the mood changes both speed *and* length

The zones are shaped two different ways. Either knob alone would leave half the
sculpture unmoved:

**Time-shaped** — character comes from timing, responds to **speed**
: Z1 mouth (peristaltic), Z2 shoulder (peristaltic), Z3 midbody (strobe)

**Space-shaped** — character comes from how many pixels are lit at once,
responds to **length**
: Z4 rear (`gradientWidth`), Z5/Z6 legs (`zapLength`), Z7 digestive
(`gradientLength`), Z8 stations (`tailLength`)

### The structural change

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

**Brightness is never touched.** Ambient draw already sits near the supply
ceiling. "Everything gets brighter" isn't available to us — and concentrating
and unifying colour reads as more dramatic than adding light anyway.
`lengthScale` is the one knob that costs current, since more consecutive lit
pixels is literally more amps. That is why **CHARGED is the short mood**, so
the two roughly balance rather than one being a step up in draw.

**Shortest-arc hue interpolation.** Hue is a circle. Interpolating 0.9 → 0.1 by
simple lerp travels backwards through green and cyan — 0.8 of the wheel instead
of 0.2. `ledComboLerpHue()` wraps the short way.

**Saturation goes to full.** Z1 renders as white (saturation 0), and hue is
meaningless on a white pixel — without this the mouth would sit unchanged while
the zones beside it changed colour. Driving to *full* rather than partway also
matters for legibility: ambient zones are dim (0.03–0.25), and at that
brightness a pastel colour is barely a colour. Saturation is the only lever
available, since brightness is off limits.

### Tuning

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

And the colour code, indexed by how many zones are lit:

```cpp
static const float COMBO_HUE[LED_MOOD_COUNT][9] = {
  //          0      1     2      3      4        5       6      7       8
  /* ORG */ {0.00f, 0.00f, 0.00f, 0.33f, 0.85f,  0.15f,  0.50f, 0.08f,  0.67f},
  //                       red    green  magenta yellow  cyan   orange  blue
  /* CHG */ {0.00f, 0.00f, 0.33f, 0.00f, 0.50f,  0.85f,  0.15f, 0.67f,  0.08f},
  //                       green  red    cyan    magenta yellow blue    orange
};
```

Button filtering lives in `Eclair_LED.ino`:

```cpp
const unsigned long TRIGGER_HOLD_MS = 150;   // continuous HIGH required
const unsigned long RELEASE_HOLD_MS = 50;
```

### Behaviour notes

**B8 also fires FIRE8.** Every mood change is punctuated by a poof — you can't
change the look quietly. That may be good or annoying; it's a Tardi-side
question, not an Eclair one.

**Mood only toggles on a clean B8 press with nothing else held.** B1+B8 is the
full-body combo, so an unconditional toggle would scramble the palette every
time the payoff runs.

**Mood speed and length affect active animations only.** The mood's *hue shift*
does apply to ambient, which is what makes B8 visible with nobody pressing.

**Convergence eases rather than snapping.** Releasing a button looks like the
sculpture relaxing instead of a light switch. Costs nothing.

### Verification

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

All pass. The mood and colour *values* are chosen to be legible and want tuning
against the real sculpture at night, which is the only place the numbers mean
anything.

### Files

| File | Role |
|---|---|
| `led_combo.h` / `.cpp` | mood table, convergence ladder, hue maths |
| `led_engine.cpp` | green override removed; combo stage added; speed scale |
| `led_z4_rear.cpp`, `led_legs.cpp`, `led_z7_digestive.cpp`, `led_z8_stations.cpp` | active length constants scaled by mood |
| `Eclair_LED.ino` | asymmetric button filter, held-zone mask, B8 mood toggle, `buttons` noise check |

---

## Arduino IDE

> **[`FLASHING.md`](FLASHING.md) is the one-page card** — settings, what each
> sketch should print on boot, and a symptom table. That is the file to open on
> playa; this README is the background.


Both: **ESP32S3 Dev Module**, **PSRAM: OPI PSRAM**, **Flash: 8MB**.

**Eclair** additionally:
- **USB CDC On Boot: ENABLED** — GPIO43 is lane Z7, so UART0 is gone
- plug into the **USB** port, not the UART port
- `build_opt.h` must appear as a second tab, or the LCD flags miss the library

**Tardi** needs Adafruit SSD1306 + Adafruit GFX. CDC setting doesn't matter —
UART0 is free.

---

## Serial commands

Both boards print a repeating status banner every 5 s, so it is on screen
whenever you open the monitor rather than 90 seconds gone.

**Eclair:** `status`, `driver`, `heap`, `buttons`, `colours`, `mood`,
`led on|off|solid`, `led ch 0..7`, `led red|green|blue`, `trigger 1..8`,
`trigger all`

`led ch N` lights one lane at a time; `led red|green|blue` checks byte order.
Both are far better for diagnosing wiring than watching an animation, which is
designed to look irregular and hides faults.

`buttons` samples every input hard for a second and reports what fraction read
high. With nothing pressed, every pin should be 0%:

```
btn  gpio  high%  verdict
  1     4    0.0  clean
  5    15   34.2  FLOATING - phantom presses
```

The status banner reports:

```
mood   : ORGANIC  hueShift 0.00  length x1.00  speed 100%
combo  : 2 held, 2 lit  hue 0.00->0.00  blend 1.00  zones[.23.....]  red
```

**Tardi:** `status`, `arm`, `counts`, `poof` (state and cooldown remaining)

---

## Safety behaviour

**Boot interlock (Tardi).** If any button reads HIGH at startup, Tardi does not
arm — FIRE outputs stay idle and the OLED blinks `NOT ARMED` until every input
has been seen LOW. Covers a wedged button, a shorted wire, or noise on a
floating input at power-up. Override with `arm` on serial.

**Latched fire indicator.** The old OLED sampled instantaneous pin state: a
100 ms pulse repeating every second, sampled every 250 ms, is caught about 10%
of the time. That is why the display read "FIRE OFF" while poofers were
visibly working — a sampling bug, not a display bug. Now latched 600 ms.

**Power cap.** `LED_POWER_LIMIT_MA` in `led_direct_output.cpp` is **2 A**, for
bench work. FastLED scales the frame down rather than let the rail sag.

---

## Known doc discrepancy

The root `README.md` and `docs/current_baseline.md` say normal FIRE pulses are
500 ms. The code has always used **100 ms** (`NORMAL_FIRE_PULSE_MS`). The code
is what shipped. Left as-is rather than silently changing fire timing — but the
docs need correcting.

---

## Still outstanding

All hardware. The software side is proven.

1. **Power and ground** — `docs/power_and_ground.md`. Do this before anything
   is flashed to live hardware. Same wiring plus a working ESP32 kills the
   ESP32, which is now also the fire controller.
2. **The SN74AHCT244 buffer board** — `docs/buffer_board.md`. Until it exists,
   strips run at 3.3 V against a 3.5 V threshold, and marginal, inconsistent
   behaviour is the expected result rather than a bug.
3. **Get off breadboard** — `docs/enclosure_review.md`. Button pull-downs and
   the OLED are on spring contacts in a dusty, vibrating installation.
4. **Bring-up in order** — `docs/preflight_checklist.md`.

Two numbers worth carrying into the power work:

- **Z7 is 525 physical LEDs, not 75.** Seven parallel strands share one data
  line. The code counts 75; the wire carries all seven. Total physical is
  **2,358**, not 2,008.
- **Ambient draw is roughly 11 A** against a 15 A supply, because legs (0.15)
  and Z7 (0.25, never dark) sit 5-8x the other zones' base brightness. One
  button press adds up to 12 A more.

Both sketches compile and have been flashed. Eclair pulls in ~3,700 lines of
module code originally written against the expander API, with call sites
renamed by script.

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
