# Enclosure Review

Assessment of the controller enclosure as photographed 2026-07-30, ranked by
risk. Based on a single photo — items marked **(verify)** are inferred and need
eyes on the hardware.

The build is functional and clearly the product of real work under time
pressure. The notes below are about surviving a week in dust and heat, which is
a different bar than working on a bench.

---

## What is in there

| Location | Item | Notes |
|---|---|---|
| Top | Long relay bank, screw terminals, heavy black/white conductors to glands | AC / poofer side **(verify)** |
| Upper | Breadboard + small modules on ribbon | modules unidentified **(verify)** |
| Upper | MB102 / HW-131 breadboard power supply | barrel jack, USB-A, yellow rail jumpers |
| Centre | ESP32-S3-DevKitC-1 in screw-terminal breakout | IO39/38/37/36/35/0/45/48/47/21/14/13... readable |
| Right | 8-channel relay module, SONGLE SRD-05VDC-SL-C, optocouplers | FIRE1-8 |
| Lower | Breadboard + OLED + resistor row | resistors are presumably the 10k button pull-downs |
| Lower | Second MB102 / HW-131 supply | |
| Perimeter | Cable glands | good — strain relief is being taken seriously |

### Correcting one thing

The two boards with barrel jacks and yellow jumpers are **MB102 / HW-131
breadboard power supply modules**. They are not level shifters and cannot be.

- Input 6.5-12 V via barrel jack, or 5 V via USB
- Two rails, each jumper-selectable 3.3 V or 5 V
- The 3.3 V rail is an **AMS1117 LDO** — a step-*down* linear regulator

Neither converts 3.3 V up to 5 V. There is no boost converter on these boards.
The fire relays work at 3.3 V because a relay optocoupler input is
**current-driven** — the ESP32 pin sinks current, and the relay board supplies
its own 5 V. Logic level never enters into it.

WS2812 `DIN` is a **voltage threshold** and gets no such free pass. See
[`buffer_board.md`](buffer_board.md).

---

## RISK 1 — Two breadboards in a fire installation

**The most likely thing to fail at the event rather than on the bench.**

Breadboard contacts are spring clips designed for desk prototyping. They loosen
with vibration, lose tension with thermal cycling, and playa dust is conductive
enough to matter in the gaps.

Currently on breadboard:

- button pull-down resistors — a lost pull-down floats an **active-HIGH** input
- the OLED — your only local diagnostic
- power rails feeding both of the above
- the unidentified upper modules

Failure mode is not clean. It is intermittent, heat- and vibration-dependent,
and it will present as a software bug.

**Action:** transfer to perfboard before the event. Soldered joints, socketed
ICs, screw terminals or JST at every boundary. This is the highest-value day of
work available.

---

## RISK 2 — MB102 supplies as deployed parts

Bench prototyping modules. The AMS1117 is a linear regulator that dissipates the
input-output difference as heat and is electrically noisy. Current capability is
modest and thermally limited.

They are powering your button pull-down references and your OLED. Noise or sag
there presents as phantom presses — and a phantom press on an active-HIGH input
with fire armed is a real problem.

**Action:** replace with a proper regulated supply off the main 5 V bus, or at
minimum verify under load and at temperature.

---

## RISK 3 — Jumper wire density

Large bundles of DuPont jumper wires with friction-fit ends throughout. DuPont
connectors have no retention — they hold by friction alone and back out under
vibration.

**Action:** for anything crossing a gland or leaving the enclosure, crimp
proper connectors with retention. For internal runs, at minimum add strain
relief and cable-tie bundles so no connector carries a mechanical load. Label
both ends.

---

## RISK 4 — Optoisolation unverified

The 8-channel module has optocouplers, but these boards ship with a **JD-VCC /
VCC jumper** installed by default. With it in place, the opto's output side
shares the ESP32's 5 V and ground — the isolation is cosmetic.

Real isolation means removing that jumper and feeding JD-VCC from a separate
supply, ground not bonded to logic ground.

This matters because you are switching solenoids inches from LED data lines.

**Action (verify):** look at the board. Ten-second check. If the jumper is
present, decide deliberately whether to pull it. Record the decision either way.

---

## RISK 5 — Snubbers

No snubbers or flyback protection visible across the relay contacts. Solenoid
coils are inductive; every switch-off kicks a voltage spike back into the
harness. Repeated arcing erodes relay contacts and the transient couples into
nearby low-voltage wiring.

Note the original schematic already annotated *"PixelBlaze Signal Voltage (needs
low noise)"* and added a 10k array for *"input noise reduction"* — noise was a
known problem before this build.

**Action (verify):** confirm whether anything is fitted at the solenoid end. If
not, RC snubbers across the contacts.

---

## RISK 6 — No visible fusing on the LED distribution

The architecture SVG shows a main 12 V fuse and a 5 V fuse. Nothing per-branch
is visible in the enclosure.

A single 15 A fuse protects the *supply*. It does not protect a zone whose
conductor shorts against the steel frame — that fault sees the full supply until
the main fuse decides to go.

**Action:** per-zone blade fuses on the +5 V bus. See
[`power_and_ground.md`](power_and_ground.md).

---

## RISK 7 — Dust sealing

Glands are fitted, which is the hard part. The remaining questions are the lid
gasket and whether every unused gland is plugged.

**Action (verify):** gasket condition, blank off unused glands, and consider a
filtered vent — a fully sealed box in that temperature swing will pump moist air
in and out through whatever gap it can find.

---

## Corrections to earlier advice

**Boot-state pull-ups on FIRE pins — downgraded.**

I previously recommended 10k pull-ups on all nine FIRE lines as a first-priority
safety fix, reasoning that the pins float high-impedance before `setup()` runs.

Looking at the actual relay boards: on this module style the `IN` pin drives the
optocoupler LED's **cathode**, with the anode pulled to the board's VCC. A
floating input gives the LED no current path, so the relay stays **off**. Safer
than I described, and none of the FIRE GPIOs (8-14, 21, 47) are ESP32-S3
strapping pins.

Adding explicit pull-ups is still cheap insurance — it makes boot behaviour
independent of which relay board is fitted — but it moves from "fix first" to
"do while you are in there."

**Stage 8 of the checklist tests this directly:** hold a button, reset the
ESP32, confirm nothing fires. That is worth more than the theory either way.

---

## Priority order

| | Item | When |
|---:|---|---|
| 1 | Power/ground rebuild per `power_and_ground.md` | before flashing Plan 1 |
| 2 | Get button pull-downs and OLED off breadboard | before the event |
| 3 | Verify JD-VCC jumper | 10 minutes, today |
| 4 | Per-zone fusing | with the power rebuild |
| 5 | Snubbers | before fire testing |
| 6 | Replace MB102s | if time allows |
| 7 | Connector retention and labelling | if time allows |
| 8 | Dust sealing review | last week |

Items 1 and 2 are the difference between a piece that works and a piece that
keeps working.

---

## Open questions

1. What are the two small modules on the **upper breadboard**? If they are level
   shifters, part of this review changes.
2. Is the **top relay bank** the AC/poofer side, or the button-input relays from
   the original schematic?
3. Is the **JD-VCC jumper** present on the 8-channel board?
4. Is there anything across the **solenoid contacts**?
5. What is the actual saved **master brightness** in `led_settings`? The current
   budget in `power_and_ground.md` assumes the config peaks.
