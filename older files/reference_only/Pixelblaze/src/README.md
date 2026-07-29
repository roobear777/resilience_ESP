# Resilience 2026

A 10-foot interactive fire-breathing tardigrade (water bear) LED sculpture. Debuted at **SOAK 2026**, now being updated for **Burning Man 2026**.

Participants press and hold physical buttons around the sculpture to bring it to life — activating LED zones across the tardigrade's body in a biologically-inspired peristaltic wave, with synchronized fire poofers.

## Hardware

| Component | Spec |
|---|---|
| Controller | PixelBlaze V3 Standard (+ 1 backup unit) |
| Output | Output Expansion Board, 8 channels |
| LEDs | 2,008 pixels total (WS2812B strips + WS2811 spools) |
| Power | 12V LiFePO4 battery → 15A buck converter → 5V |
| Buttons | 7 GPIO inputs (active-high, 10K pull-down resistors) |
| Fire | 9 poofer GPIOs via optoisolated relays |

## LED Zones

| Zone | Body Part | LEDs | Color | OE Channel |
|---|---|---|---|---|
| Z1 | Mouth | 208 | White | Ch 1 |
| Z2 | Shoulder | 325 | Cyan | Ch 2 |
| Z3 | Mid-body | 400 | Blue | Ch 3 |
| Z4 | Rear body | 300 | Purple | Ch 4 |
| Z5 | Front legs | 300 | Orange→Yellow | Ch 5 |
| Z6 | Back legs | 300 | Orange→Yellow | Ch 6 |
| Z7 | Digestive tract | 75 (x7 parallel) | Red (never dark; press = faster pulse) | Ch 7 |
| Z8 | Button-station strings | 100 (7 strings × 14) | Warm amber (press = chase toward sculpture) | Ch 0 |

## Interaction Model

- **Press & hold** a button → that zone's LEDs go from ambient to active animation
- **Hold all 7** → "payoff" mode with continuous mouth fire + coordinated leg fire dance
- **Release** → zone returns to ambient
- **Safety**: 10-second auto-cutoff per zone against wedged buttons

## Repo Structure

```
resilience-brc/
├── README.md
├── CONTEXT.md                     ← complete technical reference (source of truth)
├── .gitignore
├── pixelblaze/
│   ├── src/                       ← SOURCE OF TRUTH — modular pattern code
│   │   ├── config.js              ← control panel (LED tunables)
│   │   ├── layout.js              ← zone boundaries (must match OE config)
│   │   ├── lib/animations.js      ← shared primitives (triangle wave, peak-ramp)
│   │   ├── buttons.js             ← button pins + debounced polling
│   │   ├── zone-state.js          ← hold timers, safety latches, payoff
│   │   ├── zones/                 ← one module per zone (advance + render)
│   │   ├── fire/                  ← poofer pins, choreography, safety caps
│   │   ├── main.js                ← interactive entry point
│   │   └── main-pressure.js       ← burn-in entry point
│   ├── build.js                   ← concatenates src/ → dist/ (node build.js)
│   ├── dist/                      ← GENERATED single-file patterns (upload these)
│   │   ├── master-full.js         ← buttons + fire + LEDs (v2.1-equivalent)
│   │   ├── master-led-only.js     ← fire code stripped
│   │   └── pressure-test.js       ← all zones active, burn-in test
│   ├── mapper/
│   │   └── tardigrade-map.js      ← 2D pixel map for the web UI Mapper tab
│   └── tools/                     ← Python tools (deploy, backup, push map)
├── docs/
│   ├── modularization-plan.md     ← code architecture plan + meeting decisions
│   ├── post-soak-notes.md         ← post-SOAK issues/feedback from Whit
│   ├── hardware-diagnostic-guide.md ← repair checklist for Z2/Z5/Z6/Z7
│   └── ai-prompts.rtf             ← early AI-assisted design notes
├── simulator.html                 ← browser-based LED simulator (all zones, no hardware needed)
├── diagrams/                      ← SVGs, drawings, schematic
├── photos/                        ← build photos
├── backups/
│   └── pixelblaze-soak.pbb       ← PixelBlaze chip backup from SOAK
└── archive/                       ← old pattern versions + superseded docs
```

## Building the Patterns

The PixelBlaze pattern language has no `import`, so deployable patterns are single files
generated from the modules in `pixelblaze/src/`:

```bash
cd pixelblaze
node build.js              # build all three variants into dist/
node build.js --led-only   # or build just one
```

Edit `src/`, rebuild, then deploy straight to the PixelBlaze (test unit: http://192.168.77.91/).
Never edit `dist/` files directly.

## Deploying to the PixelBlaze

One-time setup:

```bash
python3 -m venv .venv
.venv/bin/pip install -r pixelblaze/tools/requirements.txt
```

Then:

```bash
.venv/bin/python pixelblaze/tools/deploy.py led-only --activate   # build + backup + upload + run
.venv/bin/python pixelblaze/tools/backup.py                       # timestamped .pbb into backups/
.venv/bin/python pixelblaze/tools/push_map.py                     # push the tardigrade pixel map
```

`deploy.py` overwrites the pattern with the same name on the device, so repeat deploys
don't accumulate copies. Use `--no-backup` to skip the pre-deploy backup, `--ip` for a
different unit. You can also paste `dist/` files into the web UI editor manually.

## Key Files

- **[`pixelblaze/src/`](pixelblaze/src/)** — Modular pattern source: per-zone animation modules, button input, zone state machine, fire choreography.
- **[`pixelblaze/dist/master-full.js`](pixelblaze/dist/master-full.js)** — Generated production pattern (all 7 zones, buttons, fire, safety logic).
- **[`pixelblaze/dist/master-led-only.js`](pixelblaze/dist/master-led-only.js)** — Generated LED-only pattern: identical behavior, all fire code stripped at build time.
- **[`docs/modularization-plan.md`](docs/modularization-plan.md)** — Architecture plan, build variants, and decisions from the 6/01 controls meeting.
- **[`CONTEXT.md`](CONTEXT.md)** — Complete technical reference for anyone working on LEDs, buttons, and firmware.
- **[`docs/hardware-diagnostic-guide.md`](docs/hardware-diagnostic-guide.md)** — Repair checklist for known hardware failures (Z2, Z5/Z6, Z7).
- **[`simulator.html`](simulator.html)** — Open in a browser to preview all LED animations without hardware. Hold buttons or press keys 1-7.

## Status — Post-SOAK Fixes (v2.1)

See [`docs/post-soak-notes.md`](docs/post-soak-notes.md) for Whit's original SOAK notes.

**Fixed in v2.1:**
- Z3 strobe animation bug (repeat timer was running during active wave, causing back-to-back strobes)
- Brightness increased across all zones for Burning Man visibility
- Z2/Z3/Z4 ambient animations replaced (were flat dim glows, now breathing pulses)
- Payoff fire dance implemented (was a placeholder — now bilateral wave sweep + burst)
- Button pin comments corrected (said pull-up, actually pull-down)

**Still needs hardware work:**
1. Z2 (shoulder) and Z5/Z6 (legs) — connector/wiring failures, need physical diagnosis
2. Z7 (digestive tract) — never installed at SOAK, needs hardware repair (see diagnostic guide)
3. Z3 physical shape — needs to match body roundness better
4. Full system burn-in test before Burning Man
5. Playa-proof enclosure (dust sealing)

## Tech Stack

All firmware is written in [PixelBlaze](https://electromage.com/) pattern language — a JavaScript-like language that runs on the PixelBlaze V3 microcontroller. Patterns are uploaded via the PixelBlaze web UI over WiFi.
