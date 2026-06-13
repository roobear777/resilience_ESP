# Technical Context — Resilience 2026

This document captures the full technical context for anyone working on the LED system, buttons, and PixelBlaze firmware.

## What This Sculpture Is

A 10-foot tardigrade (water bear) built on a central steel pole/spine with metal ribs, legs, and mesh skin. It stands on its rear ("butt") with legs sticking out the sides. The mouth/head is at the top with a nose bell. It breathes fire from the mouth and legs. LEDs illuminate the body in 7 biological zones, each with ambient and active animation states, plus an 8th zone of string lights running from the button stations to the structure.

## PixelBlaze Platform

[PixelBlaze V3](https://electromage.com/) is a WiFi-enabled LED controller with a JavaScript-like pattern language. Key facts:

- Patterns are edited/uploaded via the PB web UI (connect to its WiFi AP)
- Language supports `export var`, `function`, `array()`, `floor()`, `hsv()`, `rgb()`, `time()`
- `beforeRender(delta)` runs once per frame; `delta` is ms since last frame
- `render(index)` runs once per pixel per frame; call `hsv()` or `rgb()` to set color
- GPIO via `pinMode()`, `digitalRead()`, `digitalWrite()`
- Output Expansion Board provides 8 independent LED output channels
- Each channel has its own start index in the pixel buffer (configured in PB web UI)
- `.pbb` files are full chip backups (restore via PB web UI)

## Hardware Layout

### Output Expansion Board Channel Map

```
Ch 0: 100 LEDs   Z8 strings    — 7 button-station strings × 14 px (start index 1908)
Ch 1: 208 LEDs   Z1 mouth      — 4 tapered strips: 33, 50, 60, 65
Ch 2: 325 LEDs   Z2 shoulder   — 6 tapered strips: 44, 48, 52, 56, 60, 65
Ch 3: 400 LEDs   Z3 mid-body   — 8 rings × 50 LEDs
Ch 4: 300 LEDs   Z4 rear body  — 12 strips × 25 LEDs
Ch 5: 300 LEDs   Z5 front legs — 4 legs × 75 LEDs
Ch 6: 300 LEDs   Z6 back legs  — 4 legs × 75 LEDs
Ch 7:  75 LEDs   Z7 digestive  — 7 strands wired in parallel (share 1 data line)
────────────────
Total: 2,008 pixels
```

### Pixel Index Ranges (in pattern code)

```
Z1:    0 –  207   (208 px)
Z2:  208 –  532   (325 px)
Z3:  533 –  932   (400 px)
Z4:  933 – 1232   (300 px)
Z5: 1233 – 1532   (300 px)
Z6: 1533 – 1832   (300 px)
Z7: 1833 – 1907   ( 75 px)
Z8: 1908 – 2007   (100 px)  ← button-station strings, OE Ch 0
```

### LED Hardware

- **Zones 1, 2, 5, 6**: WS2812B strips (30 LEDs/m, GRB color order, 3-wire)
- **Zones 3, 4, 7**: WS2811 spools (S7301, 20 LEDs/m, 5cm spacing, 4-wire)
  - Wire assignments: 1=data out, 2=+5V (black dots), 3=data in, 4=GND
  - Comb wiring topology: central power spines, data daisy-chained between segments

### Button GPIO Pins

```
Button 1: GPIO 34   (input-only, needs external 10K pull-down)
Button 2: GPIO 35   (input-only, needs external pull-down)
Button 3: GPIO 36   (input-only, needs external pull-down)
Button 4: GPIO 39   (input-only, needs external pull-down)
Button 5: GPIO 25
Button 6: GPIO 26
Button 7: GPIO 27
```

Buttons are **active-high**: relay supplies 3.3V when pressed. All require external 10K pull-down resistors to GND. GPIO 34/35/36/39 are input-only on ESP32 (no internal pull resistors).

### Poofer GPIO Pins

```
Poofer 0: GPIO 33   Z1 mouth
Poofer 1: GPIO 14   Z5 leg A
Poofer 2: GPIO 16   Z5 leg B
Poofer 3: GPIO 17   Z5 leg C
Poofer 4: GPIO  4   Z5 leg D
Poofer 5: GPIO 19   Z6 leg A
Poofer 6: GPIO 21   Z6 leg B
Poofer 7: GPIO 22   Z6 leg C
Poofer 8: GPIO 13   Z6 leg D
```

Reserved pins (DO NOT USE): IO5, IO12, IO15, IO18 (SCK), IO23 (MOSI), IO32.
Spare pins: IO0, IO2.

### Power System

- 12V LiFePO4 battery (600–1200 Wh)
- 15A buck converter → 5V (considering upgrade to 20A)
- Peak draw: ~90W (~12–14A at 5V across all zones active)
- Average draw: ~45–50W (animations designed around 50% brightness)
- Runtime target: 8+ hours

## Zone Animations

### Zone Colors (PixelBlaze HSV, hue 0–1)

| Zone | Color | Hue | Saturation |
|---|---|---|---|
| Z1 | White | 0 | 0 |
| Z2 | Cyan | 0.5 | 1.0 |
| Z3 | Blue | 0.67 | 1.0 |
| Z4 | Purple | 0.917 | 1.0 |
| Z5 | Orange→Yellow | 0.03→0.12 | 1.0 |
| Z6 | Orange→Yellow | 0.03→0.12 | 1.0 |
| Z7 | Red | 0.0 | 1.0 |
| Z8 | Warm amber | 0.08 | 0.85 |

### Per-Zone Animation Summary

- **Z1 (mouth)**: Ambient = sequential pulse across 4 strips. Active = peristaltic wave, strips fire in sequence with fast peak + slow ramp-down.
- **Z2 (shoulder)**: Ambient = slow cyan breathing pulse (3s cycle). Active = peristaltic wave across 6 strips top-to-bottom, supports 2 overlapping cycles.
- **Z3 (mid-body)**: Ambient = deep blue breathing pulse (3.3s cycle). Active = center-out strobe: rings 3-4 strobe first, then 2-5, 1-6, 0-7. Strobe brightness decreases each cycle.
- **Z4 (rear body)**: Ambient = slow purple breathing pulse (4s cycle). Active = bright gradient pulse falls down each of 12 strips, emanating from center strips outward.
- **Z5/Z6 (legs)**: Ambient = alternating pulse walk across odd/even legs. Active = fire-zap: bright peak sweeps down each leg with trailing orange→yellow gradient.
- **Z7 (digestive)**: Ambient = gradient pulse falls top→bottom (2.5s cycle). Active = same pulse as rapid peristalsis (0.8s cycle, brighter baseline). Never dark — the ambient pulse always runs.
- **Z8 (station strings)**: 7 strings × 14 px, one per button station, running station → structure. Ambient = warm amber breathing, phase-staggered per station. Active (that station's button held) = bright chase flowing from the button toward the sculpture.

### Interaction State Machine

Each button independently controls its zone:
1. Button pressed → zone goes active
2. Button released → zone returns to ambient
3. Hold > 10 seconds → zone force-latches off (safety, resets on release)
4. All 7 held → "payoff" mode layered on top (continuous mouth fire, coordinated leg fire dance)

### Safety Caps

| Parameter | Value | Purpose |
|---|---|---|
| `zoneMaxHoldMs` | 10,000 ms | Single zone auto-cutoff |
| `payoffMaxHoldMs` | 10,000 ms | All-7 payoff auto-cutoff |
| `level7MaxMs` | 5,000 ms | Z1 poofer during payoff |
| `pooferMaxOnMs` | 500 ms | Per-poofer max pulse |
| `debounceMs` | 30 ms | Button debounce |

## Master Pattern Architecture (v3 — modular)

The pattern source lives in **`pixelblaze/src/`** as modules; the PixelBlaze language has no
`import`, so `pixelblaze/build.js` concatenates them into single-file patterns in
`pixelblaze/dist/` (see [`docs/modularization-plan.md`](docs/modularization-plan.md)):

| Module | Contents |
|---|---|
| `src/config.js` | Control panel — all LED tunables + `globalBrightness` |
| `src/layout.js` | `ZONE_OFFSETS` pixel boundaries (must match OE config) |
| `src/lib/animations.js` | Shared primitives: `triangleWave` (breathing), `peakRampBrightness` (peristaltic envelope) |
| `src/buttons.js` | Button pin map, `buttonsConnected` gate, debounced polling |
| `src/zone-state.js` | Per-zone hold timers, 10s safety latches, all-7 payoff detection |
| `src/zones/*.js` | One module per zone: `advance()` + `renderAmbient()`/`renderActive()` |
| `src/fire/config.js` | Poofer tunables, `poofersArmed` gate, fire safety caps |
| `src/fire/poofers.js` | Poofer pin map, choreography, `level7Dance`, pin writes |
| `src/main.js` | `beforeRender` frame loop + `render` pixel dispatch |
| `src/main-pressure.js` | Burn-in entry point (all zones forced active) |

Build variants (`node build.js [--full|--led-only|--pressure]`):

- **`dist/master-full.js`** — v2.1-equivalent: buttons + state machine + fire + LEDs
- **`dist/master-led-only.js`** — all fire code stripped at build time; no poofer GPIO is ever configured or driven
- **`dist/pressure-test.js`** — every zone active continuously, no buttons/fire (burn-in)

The last single-file version (v2.1, as run at SOAK) is preserved at
[`archive/master-pattern-v2.1.js`](archive/master-pattern-v2.1.js).

A browser-based **LED simulator** ([`simulator.html`](simulator.html)) executes the actual `dist/` builds inside a PixelBlaze API shim for desk-side iteration without hardware.

A 2D **pixel map** for the web UI Mapper tab lives at
[`pixelblaze/mapper/tardigrade-map.js`](pixelblaze/mapper/tardigrade-map.js) (side-view
tardigrade: snout arcs, body rings, rear strips, hanging legs, digestive strand, station
strings along the ground). Push it with `tools/push_map.py`; re-push after changing the
device pixel count.

Two global safety gates (in addition to the `--led-only` build, which removes fire code entirely):
- `buttonsConnected` — set to 0 for testing without buttons (all zones stay ambient)
- `poofersArmed` — set to 0 to disable all fire output regardless of state

## SOAK Post-Mortem Issues

From Whit's post-SOAK notes ([`docs/post-soak-notes.md`](docs/post-soak-notes.md)):

1. **Z2 (shoulder)** stopped working at the event — was functional before
2. **Z5/Z6 (4+ legs)** stopped working — were functional before
3. **Z3 (mid-body)** animation was wrong — only base color showed, lost the downward wave motion
4. **Z7 (digestive tract)** never installed:
   - 3-wire (black) version: suspected short at a silicon tube joint
   - 4-wire (white) version: wired backwards (data direction reversed)
5. **Brightness too low** — global brightness was conservatively tuned down; battery life was fine
6. **Z3 physical shape** — needs to open up more to match the tardigrade's body roundness

## Pattern Version History

| Version | File | Architecture |
|---|---|---|
| 1.0 | [`archive/master-pattern-v1.0.js`](archive/master-pattern-v1.0.js) | Sequential `sequenceLevel` state machine |
| 1.1 | [`archive/master-pattern-v1.1.js`](archive/master-pattern-v1.1.js) | Iteration on sequential model |
| 1.2 | [`archive/master-pattern-v1.2.js`](archive/master-pattern-v1.2.js) | Transition toward per-zone independence |
| 1.3 | [`archive/master-pattern-v1.3.js`](archive/master-pattern-v1.3.js) | Per-zone buttons, initial payoff logic |
| 2.0 | `archive/master-pattern-v2.0.js` | Independent per-zone activation, all-7 payoff, placeholder ambients (SOAK version) |
| 2.1 | [`archive/master-pattern-v2.1.js`](archive/master-pattern-v2.1.js) | Fixed Z3 strobe timing, real ambient animations, bilateral fire dance, brighter for Burning Man |
| **3.0** | [`pixelblaze/src/`](pixelblaze/src/) → `pixelblaze/dist/` | **Current.** Modular source + build step; behavior identical to 2.1 plus: Z7 active state, new Z8 station-string zone, shared animation lib; adds `--led-only` and `--pressure` build variants |

## Burning Man TODO

- [x] Fix Z3 animation (fixed strobe timing bug — repeat timer was running during active wave)
- [x] Increase brightness / make animations bolder (all zones bumped)
- [x] Replace placeholder ambient animations (Z2/Z3/Z4 now have breathing pulses)
- [x] Design payoff fire choreography (bilateral wave sweep + burst)
- [ ] Diagnose and fix Z2, Z5/Z6 hardware failures (see `docs/hardware-diagnostic-guide.md`)
- [ ] Fix or rebuild Z7 digestive tract (see `docs/hardware-diagnostic-guide.md`)
- [ ] Full system burn-in test
- [ ] Playa-proof enclosure (dust sealing)
