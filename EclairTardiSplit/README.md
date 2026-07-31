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
  solenoid harness; invisible to a real press and to the 30 ms debounce.
- **Common ground between the boxes** on a real conductor.
- **Twisted pair** per button, signal with its own ground.

Both sketches count rejected glitches and report them. A climbing glitch count
on the fire board is worth acting on — there the failure mode is unintended
fire.

---

## Behaviour

| Input | Tardi | Eclair |
|---|---|---|
| Button 1-7 | FIRE1-7 pulse while held | triggers Z1-Z7 |
| Button 8 | FIRE8 | no zone of its own |
| **B1 + B8** | FIRE9 head poof, 10 s cutoff | full-body: all zones |
| **B2 + B6** | — | everything green |
| All 8 | one coordinated 500 ms pulse | — |

A press **restarts** a zone's window rather than being ignored while it is
already active, so a press always gets a response.

---

## Arduino IDE

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

**Eclair:** `status`, `driver`, `heap`, `led on|off|solid`, `led ch 0..7`,
`led red|green|blue`, `trigger 1..8`, `trigger all`

`led ch N` lights one lane at a time; `led red|green|blue` checks byte order.
Both are far better for diagnosing wiring than watching an animation, which is
designed to look irregular and hides faults.

**Tardi:** `status`, `arm`, `counts`

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
500 ms. The code has always used **100 ms** (`NORMAL_FIRE_PULSE_MS`); only the
all-8 coordinated pulse is 500 ms. The code is what shipped. Left as-is rather
than silently changing fire timing — but the docs need correcting.

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

Neither sketch has been compiled. Eclair pulls in ~3,700 lines of module code
originally written against the expander API, with call sites renamed by script
— expect a few errors on the first build.
