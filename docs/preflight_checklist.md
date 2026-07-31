# Pre-Flight Hardware Checklist

Ordered bring-up for the direct-drive build. Each stage has a **go/no-go** —
do not proceed past a failed check.

The ordering principle: **energise the cheapest thing first, and never connect
a new subsystem to a live one.** Most hardware in this project has died during
plug/unplug operations on powered systems.

---

## Stage 0 — Before any power

Meter only. Battery disconnected.

- [ ] `power_and_ground.md` topology is actually built — bus bars exist, each
      zone has its own pair back to them
- [ ] Continuity: every zone connector to the +5 V bar and the GND bar
- [ ] **No continuity** between +5 V bar and GND bar anywhere
- [ ] Exactly **one** bond between LED power ground and logic ground. Lift the
      logic ground and confirm continuity disappears
- [ ] Per-zone fuses fitted and correctly rated
- [ ] No LED conductor passes through the ESP32 board, the buffer board, or a
      breadboard rail
- [ ] Data cables physically separated from the 120 VAC solenoid harness
- [ ] Relay board **JD-VCC jumper**: removed and JD-VCC fed separately if real
      optoisolation is wanted. Record the answer either way — "we checked and
      left it on" is a valid outcome; "we never looked" is not

**No-go:** any short between rails, or more than one ground bond.

---

## Stage 1 — Supply only, no loads

Battery in. Nothing else connected.

- [ ] Buck output measures 5.0-5.2 V unloaded
- [ ] Ripple looks sane on a scope if one is available
- [ ] Main fuse and 5 V fuse both correctly rated and seated

**No-go:** output outside 4.9-5.3 V.

---

## Stage 2 — Logic only

ESP32 and buffer board powered. **No LED strips, no fire connections.**

- [ ] 5.0 V at buffer pin 20, 0 V at pin 10
- [ ] Buffer chip is not warm
- [ ] ESP32 boots, serial console reachable **over native USB** (GPIO19/20 —
      *not* the UART bridge port; GPIO43 is now Z7's data line)
- [ ] `FastLED direct backend: READY` appears
- [ ] `ROUTING CONFIRMED: 7/7 real lanes use LCD_CLOCKLESS`
- [ ] First-show heap report shows `internal_dma_largest` comfortably above the
      allocation. **This is the most likely software-side failure of the new
      driver** — it fails loudly at the first `FastLED.show()`
- [ ] OLED lights (if moved to GPIO38/44)

**No-go:** `ROUTING FAILED`, or a DMA allocation error. Stop and fix before
touching hardware — this is a firmware problem, not a wiring one.

---

## Stage 3 — Buffer outputs, no strips

Still no LEDs connected.

- [ ] `led ch 0` .. `led ch 6` through each lane in `VALIDATE_CHANNEL` mode
- [ ] Each lane's output pin shows activity, ~5 V swing, idle low
- [ ] Inactive lanes stay quiet
- [ ] All seven lanes verified before any strip is attached

**No-go:** a lane stuck high, stuck low, or swinging to 3.3 V instead of 5 V.
3.3 V means VCC is on the wrong rail.

---

## Stage 4 — One short test strip

A short strip on a bench lead. **Power down before connecting.**

> **Never hot-plug a strip.** If the data pin makes contact before ground, the
> return current path is through the data pin. This is the most common way
> these drivers die, and it is very likely what killed the expander during the
> "which outputs work" testing.

- [ ] Powered down, strip connected to lane 1
- [ ] Power up. `VALIDATE_SOLID` — all pixels light
- [ ] `VALIDATE_COLOR red` shows **red**, green shows green, blue shows blue.
      Wrong colours here mean the channel colour order is wrong, not the wiring
- [ ] Repeat on each lane, powering down between every change

**No-go:** wrong colours, or dead pixels partway along.

---

## Stage 5 — Zones one at a time

Real strips, one zone per session, powered down between each.

For each zone Z1..Z7:

- [ ] Powered down. Zone connected to its lane and its own fused bus feed
- [ ] Power up. `VALIDATE_CHANNEL` for that zone — full length lights
- [ ] **Measure 5 V at the far end under load. Above 4.5 V**
- [ ] No colour drift along the run. Warm drift / fading blue = voltage sag,
      add injection
- [ ] Clamp-meter the zone's feed. Compare against the table in
      `power_and_ground.md`

Zone-specific:

- [ ] **Z3 (400 px)** — the longest lane, and the one that exercises the
      driver's multi-chunk transmission path. Also the zone whose animation was
      wrong at SOAK. Test this one carefully
- [ ] **Z7** — confirm all 7 parallel strands light together, and that the
      power budget accounts for **525 physical LEDs**, not 75
- [ ] **Z5/Z6** — confirm each leg is fed independently from the bus, not
      daisy-chained. Multiple legs dying together at SOAK points at a shared
      feed

**No-go:** below 4.5 V at any far end, or any zone drawing more than ~1.5x its
predicted current.

---

## Stage 6 — All zones, ambient

Everything connected. No fire.

- [ ] Ambient animation runs on all seven zones
- [ ] Total draw measured and compared against the ~11 A ambient estimate
- [ ] Rail holds above 4.8 V at the bus under ambient
- [ ] Run 30 minutes. Nothing warm that should not be: check fuses, terminals,
      buck converter, buffer chip
- [ ] No flicker, no dropouts, no resets

**No-go:** any reset, or rail sag below 4.7 V at idle.

---

## Stage 7 — Interaction, still no fire

`FIRE_OUTPUTS_ENABLED = false`.

- [ ] Each button triggers its zone
- [ ] Rail holds above 4.7 V with a zone at peak
- [ ] Button 1 + Button 8 triggers full-body animation
- [ ] **Measure peak draw during full-body.** This is the worst case and the
      number that determines whether the supply is adequate
- [ ] No phantom triggers when nothing is pressed — watch for 60 seconds with
      the solenoid harness energised but fire disabled

**No-go:** phantom button events, or brownout during full-body.

---

## Stage 8 — Fire, isolated

Fire enabled. **Gas off. Solenoids connected, no fuel.**

- [ ] Each button produces the expected solenoid click
- [ ] OLED / serial reports fire state matching what actually clicked
- [ ] Pulse durations match the code, not the docs — code says
      `NORMAL_FIRE_PULSE_MS = 100`, README says 500. Confirm which is real
- [ ] Head Poof (B1+B8) 10-second cutoff verified with a stopwatch
- [ ] **Hold a button and reset the ESP32.** Nothing should fire during boot
- [ ] **Brown out the supply while a button is held.** Nothing should latch on
- [ ] Watch for LED glitching when solenoids switch — that is inductive
      coupling into the data lines, and it means snubbers or better separation

**No-go:** any fire on boot, any latched output, or LED corruption on solenoid
switching.

---

## Stage 9 — Full system soak

- [ ] Everything live, 2+ hours
- [ ] Periodic button exercise
- [ ] Nothing above ambient temperature that should not be
- [ ] No resets, no dropouts
- [ ] Re-torque every screw terminal afterwards — thermal cycling loosens them

---

## Field kit

Things that fail out there, and what makes them survivable:

- [ ] Spare **SN74AHCT244** (socketed, so it is a 10-second swap)
- [ ] Spare fuses, every value used
- [ ] Spare ESP32-S3, **pre-flashed and tested**
- [ ] Output Expander as a backup path, wired data-and-ground only
- [ ] Multimeter
- [ ] Crimps, connectors, heatshrink
- [ ] A printed copy of `gpio_schema.md` and `power_and_ground.md`
- [ ] Laptop with the toolchain, or a known-good build on a USB stick

---

## The two rules worth repeating

1. **Power down before connecting or disconnecting any strip.**
2. **No LED current through any PCB.**

Between them these cover both known board losses on this project.
