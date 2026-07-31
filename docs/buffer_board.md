# Buffer Board — SN74AHCT244 Output Stage

The Output Expander did three jobs: **routing**, **level shifting**, and
**series termination**. In the direct-drive build the routing moved into
firmware (framebuffer slicing in `led_direct_output.cpp`). This board restores
the other two.

It is not a new component. It is the part of the expander that is not being
replaced by software.

## Why it is required

| Input type | Mechanism | Works at 3.3 V? |
|---|---|---|
| Relay optocoupler (FIRE) | current sink to ground | **yes** |
| WS2812 `DIN` | voltage threshold vs. its own supply | **no** |

WS2812B needs `VIH >= 0.7 x VDD`. At a 5 V supply that is **3.5 V**. An ESP32-S3
GPIO drives 3.3 V — under spec, with no margin, before ground offset is
subtracted.

The fire relays working fine at 3.3 V does not transfer to the LEDs. They are
different kinds of input.

The expander was supplying 5 V signalling all along:

> The outputs are level-shifted to 5v with 100 ohm impedance matching resistors
> for twisted pair like found in CAT 5/CAT 6.
> — ElectroMage Output Expander product description

## Part selection

**SN74AHCT244N** — octal buffer / line driver, DIP-20.

The **T** is not optional. It means TTL-compatible input thresholds: anything
above **2.0 V reads HIGH**, and that threshold does not scale with the supply
rail. Power the chip from 5 V, feed it 3.3 V, get 5 V out.

| Part | Input threshold at 5 V | Usable here |
|---|---|---|
| 74**AHCT**244 | 2.0 V (TTL) | **yes** |
| 74**HCT**244 | 2.0 V (TTL) | yes, slower edges |
| 74HC244 | 3.5 V (CMOS) | **no** |
| 74AHC244 | 3.5 V (CMOS) | **no** |

Ordering a plain HC part is the most common mistake with this chip. It will
appear to work on a short bench lead and fail unpredictably.

`'244` (unidirectional buffer) is preferred over `'245` (bidirectional
transceiver) — no direction pin to get wrong.

## Bill of materials

| Qty | Part | Note |
|---:|---|---|
| 1 | SN74AHCT244N, DIP-20 | buy 3 — they are pennies and it is a sacrificial part |
| 1 | 0.1 uF ceramic capacitor | decoupling, mounted at the chip |
| 1 | 10 uF electrolytic or ceramic | bulk, optional but cheap |
| 7 | 100 ohm resistor, 1/4 W | series termination, one per lane |
| 1 | 20-pin DIP socket | so a blown chip is a 10-second swap |
| 1 | Perfboard + screw terminals or JST | **not breadboard** |

## Pinout

The `'244` **interleaves** inputs and outputs down both sides. They are not
grouped. This is the second most common mistake with this chip.

```
            SN74AHCT244N
           +------\/------+
    1OE  1 |o             | 20  VCC   -> +5 V
   1A1   2 |              | 19  2OE
   2Y4   3 |              | 18  1Y1
   1A2   4 |              | 17  2A4
   2Y3   5 |              | 16  1Y2
   1A3   6 |              | 15  2A3
   2Y2   7 |              | 14  1Y3
   1A4   8 |              | 13  2A2
   2Y1   9 |              | 12  1Y4
   GND  10 |              | 11  2A1
           +--------------+
```

## Lane wiring

| Lane | Zone | ESP32 GPIO | -> input pin | -> output pin | -> 100R -> |
|---:|---|---:|---|---|---|
| 1 | Z1 mouth | GPIO1 | `1A1` (2) | `1Y1` (18) | Z1 DIN |
| 2 | Z2 shoulder | GPIO2 | `1A2` (4) | `1Y2` (16) | Z2 DIN |
| 3 | Z3 midbody | GPIO39 | `1A3` (6) | `1Y3` (14) | Z3 DIN |
| 4 | Z4 rear | GPIO40 | `1A4` (8) | `1Y4` (12) | Z4 DIN |
| 5 | Z5 front legs | GPIO41 | `2A1` (11) | `2Y1` (9) | Z5 DIN |
| 6 | Z6 back legs | GPIO42 | `2A2` (13) | `2Y2` (7) | Z6 DIN |
| 7 | Z7 digestive | GPIO43 | `2A3` (15) | `2Y3` (5) | Z7 DIN (x7 parallel) |
| — | unused | — | `2A4` (17) **-> GND** | `2Y4` (3) leave open | — |

## Power and control pins

| Pin | Connect to | Why |
|---|---|---|
| 20 `VCC` | **+5 V bus** | this rail sets the output voltage — must be the same 5 V the LEDs run on |
| 10 `GND` | **ground bar**, single bond point | see `power_and_ground.md` |
| 1 `1OE` | **GND** | active-low output enable |
| 19 `2OE` | **GND** | active-low output enable |
| 17 `2A4` | **GND** | never float an unused CMOS input |

**Three ways to get a dead board that looks correctly wired:**

1. `1OE` / `2OE` left floating — outputs stay high-impedance, nothing lights.
2. `2A4` left floating — the unused input oscillates, drawing current and
   injecting noise into the shared package.
3. VCC fed from 3.3 V instead of 5 V — the chip works perfectly and outputs
   3.3 V, accomplishing nothing.

## Decoupling

The 0.1 uF ceramic goes **between pin 20 and pin 10, physically at the chip** —
short leads, not across the board.

Eight outputs switching at 800 kHz into cable capacitance draw current in sharp
bursts. That capacitor is the local reservoir. Without it the chip's own supply
sags on every edge and the outputs get soft, which defeats the point of fitting
a buffer.

Add the 10 uF in parallel for bulk if convenient.

## Construction

**Do not build this on breadboard.** It carries the signal for every LED on the
sculpture, and breadboard spring contacts loosen with vibration and oxidise with
dust. See `enclosure_review.md`.

- Socket the chip. A blown buffer becomes a 10-second field swap.
- Screw terminals or JST connectors for the 7 outgoing lanes, labelled Z1-Z7.
- 100 ohm resistors at the **board end**, not the strip end — series termination
  damps the reflection at the source.
- Mount it close to the ESP32. The 3.3 V side should be short; the 5 V side is
  the one that tolerates length.
- Data out to each zone as **twisted pair with its own ground** — CAT5 is ideal
  and matches the 100 ohm.

## Bring-up

1. Chip **out** of its socket. Power up. Verify 5.0 V at pin 20, 0 V at pin 10.
2. Power down. Insert chip. Power up. Confirm it is not warm — a warm '244 with
   no load means a wiring fault.
3. With firmware in `VALIDATE_CHANNEL` mode on one lane, scope or meter that
   lane's output. Idle low, active showing a ~5 V swing.
4. Connect one short test strip to one lane. Confirm before wiring all seven.
5. Only then connect full-length runs.

Full sequence in [`preflight_checklist.md`](preflight_checklist.md).

## Failure modes to recognise

| Symptom | Likely cause |
|---|---|
| No lanes work at all | `OE` pins floating, or VCC not connected |
| All lanes 3.3 V | VCC on the 3.3 V rail instead of 5 V |
| Works short, fails long | missing/wrong series resistor, or shared ground offset |
| One lane dead | that lane's buffer channel blown — swap the socketed chip |
| Random flicker across all lanes | missing decoupling cap, or 5 V rail sagging |
| First pixel of a strip damaged | reflection overshoot — verify the 100 ohm is fitted |
