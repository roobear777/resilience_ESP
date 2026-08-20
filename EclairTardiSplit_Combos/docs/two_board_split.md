# Two-Board Split — Tardi (fire) and Eclair (LED)

Replaces the single `esp32_controller` firmware. Two independent ESP32-S3
boards, two independent sketches, no communication link between them.

| | `Tardi_Fire/` | `Eclair_LED/` |
|---|---|---|
| Job | buttons, FIRE1-9, OLED | buttons, Z1-Z8 animation, WiFi tuning |
| Buttons | 8 inputs | 8 inputs — **the same wires** |
| Outputs | 9 relay lines, active-LOW | 8 WS2812 lanes via LCD_CLOCKLESS |
| OLED | yes, GPIO1/2 | no |
| WiFi | **none** | TARDI-LED AP |
| Enclosure | existing box | new box, near the sculpture |

## Why split by function, not by zone

The enclosure is full, so the LED driver had to move to a second box regardless.
Given that, splitting by function buys three things:

**No link to fail.** The button wires land on both boards. Both see the same
edge at the same instant. There is no I2C, no UART, no protocol — the buttons
*are* the synchronisation mechanism. If the "link" breaks, a button has broken,
and a multimeter finds that in ten seconds.

This is why the earlier I2C-between-boards proposal was rejected. I2C is
open-drain, bidirectional, and specified for roughly 400 pF of bus capacitance
— a few feet. Over 20+ feet past switching solenoids it fails intermittently.
If two boards ever *do* need a link, use RS-485 or CAN, not I2C.

**All zones stay on one chip.** The animations sweep *across* zones — the
peristaltic wave, the full-body payoff. Splitting Z1-Z3 onto one board and
Z4-Z7 onto another would need permanent phase-locking over a link. Splitting by
function doesn't.

**Fire gets a dedicated controller.** Tardi is a few hundred lines with no
WiFi, no DMA, no frame loop. Previously one chip ran WiFi, an HTTP server, an
OLED, ~2,000 pixels of DMA and nine relays — and the LED write blocked the loop
for tens of milliseconds, which is exactly how long a solenoid stays open past
when it should have closed.

## Z8 came back

The single-board direct-drive build dropped Z8 (button-station strings) —
GPIO1/2 were the OLED, and there was no eighth lane left. With the OLED on
Tardi, **GPIO38 is free on Eclair** and Z8 has a real lane again.

Total is back to 2,008 logical pixels across 8 zones.

## Pin maps

### Tardi (fire)

| Function | GPIO |
|---|---|
| Buttons 1-8 | 4, 5, 6, 7, 15, 16, 17, 18 |
| FIRE1-8 | 8, 9, 10, 11, 12, 13, 14, 21 |
| FIRE9 / head poof | 47 |
| OLED SDA / SCL | 1, 2 |

UART0 (GPIO43/44) is free — one of the pins the split gave back. Serial works
from either USB port.

### Eclair (LED)

| Function | GPIO |
|---|---|
| Buttons 1-8 | 4, 5, 6, 7, 15, 16, 17, 18 |
| Z1 mouth (208 px) | 1 |
| Z2 shoulder (325 px) | 2 |
| Z3 midbody (400 px) | 39 |
| Z4 rear (300 px) | 40 |
| Z5 front legs (300 px) | 41 |
| Z6 back legs (300 px) | 42 |
| Z7 digestive (75 px) | 43 |
| Z8 stations (100 px) | 38 |
| LCD driver padding | 0 — **unwired** |

Free after all of that: GPIO3, GPIO44.

**Button pins are identical on both boards on purpose.** One physical wire per
button reaches both boards on the identically-numbered pin. Keep this
invariant; it is what makes the split debuggable.

## Shared button wiring

Per button, one wire feeding two boards. ESP32 inputs are high-impedance CMOS
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
- **1k series at each GPIO** — limits backfeed current if one board is powered
  and the other isn't. ESP32 pins have protection diodes to VDD, so driving a
  pin on an unpowered chip can partially power it through them.
- **100nF to ground at each input** — 1k x 100nF = 100 us. Kills RF pickup from
  the solenoid harness; invisible to a real press and to the 30 ms software
  debounce.
- **Common ground between the boxes** on a real conductor.
- **Twisted pair** per button, signal with its own ground.

Phantom presses matter more here than anywhere else, because on Tardi a phantom
press means fire. Both sketches count rejected glitches and report them — a
climbing glitch count on the fire board is worth acting on.

## Boot safety interlock (Tardi)

If any button reads HIGH at startup, Tardi does **not** arm. All FIRE outputs
stay idle and the OLED blinks `NOT ARMED` until every input has been seen LOW
at least once.

Covers a wedged button, a shorted wire, and noise on a floating input at
power-up — any of which would otherwise mean fire the moment the board wakes.

Override from serial with `arm` if you need to. Disable entirely with
`ENABLE_BOOT_INTERLOCK = false`.

## OLED fire indicator

The old firmware sampled instantaneous pin state: a 100 ms pulse repeating
every 1000 ms, sampled every 250 ms, is caught about 10% of the time. That is
why the display read "FIRE OFF" while the poofers were visibly working — not a
display bug, a sampling bug.

The indicator is now latched for `FIRE_INDICATOR_HOLD_MS` (600 ms) after any
fire activity, so it shows what a human sees.

## Known doc discrepancy

`README.md` and `docs/current_baseline.md` both say normal FIRE pulses are
500 ms. The code has always used **100 ms** (`NORMAL_FIRE_PULSE_MS`). Only the
all-8 coordinated pulse is 500 ms. The code is what shipped; the docs need
correcting.

## Arduino IDE

Both boards: **ESP32S3 Dev Module**, **PSRAM: OPI PSRAM**, **Flash: 8MB**.

**Eclair** additionally needs:
- **USB CDC On Boot: ENABLED** — GPIO43 is lane Z7, so UART0 is gone
- plug into the **USB** port, not the UART port
- `build_opt.h` must appear as a second tab, or the LCD driver flags don't
  reach the library

**Tardi** needs Adafruit SSD1306 + Adafruit GFX. CDC setting doesn't matter —
UART0 is free.

## Serial commands

**Eclair:** `status`, `driver`, `heap`, `led on|off|solid`, `led ch 0..7`,
`led red|green|blue`, `trigger 1..7`, `trigger all`

`led ch N` lights one lane at a time and `led red|green|blue` checks byte order
— both far better for diagnosing wiring than watching an animation, which is
designed to look irregular and hides faults.

**Tardi:** `status`, `arm`, `counts`

Both print a repeating status banner every 5 s, so it is on screen whenever you
open the monitor rather than 90 seconds gone.

## Still outstanding

Hardware, not software:

- The **SN74AHCT244 buffer board** — see `buffer_board.md`. Until it exists,
  strips are driven at 3.3 V against a 3.5 V threshold and marginal behaviour
  is expected, not a bug.
- **Power and ground** — see `power_and_ground.md`. The `LED_POWER_LIMIT_MA`
  cap in `led_direct_output.cpp` is set to 2 A for bench work. The sculpture is
  **2,358 physical LEDs**, not 2,008, because Z7 is seven parallel strands off
  one data line.
- Bring-up order in `preflight_checklist.md`.
