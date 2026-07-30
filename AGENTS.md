# Tardi Controller Working Rules

## Current Hardware Build

The live Eclair7 firmware uses two ESP32-S3-DevKitC-1 / WROOM-1-N8R8 boards.
Tardi retains buttons, FIRE, web/settings, and interaction authority. Eclair
renders and drives all seven LED zones. The Pixelblaze Output Expander, OLED,
Z8/button-station LEDs, and physical setup button are not active hardware.

```text
Tardi GPIO40 TX -> Eclair GPIO18 RX
Tardi GPIO41 RX <- Eclair GPIO17 TX
Eclair GPIO4    -> Z1 Mouth, 208 pixels
Eclair GPIO5    -> Z2 Shoulder, 325 pixels
Eclair GPIO6    -> Z3 Midbody, 400 pixels
Eclair GPIO7    -> Z4 Rear, 300 pixels
Eclair GPIO8    -> Z5 Front legs, 300 pixels
Eclair GPIO9    -> Z6 Back legs, 300 pixels
Eclair GPIO10   -> Z7 Digestive, 75 pixels
```

Total logical and physical pixels: 1,908. Wire order is GRB. Z3 must remain a
full 400-pixel animation lane so the pinned FastLED multi-chunk implementation
can be validated on hardware.

Do not restore OLED, Output Expander output, Z8, or GPIO40 setup-button
ownership unless explicitly requested.

## Critical FIRE Rules

Do not change FIRE GPIOs, active-LOW polarity, or cutoff behavior casually.

```text
idle      = HIGH
triggered = LOW
return    = HIGH
```

Current accepted proof-of-concept behavior:

- FIRE1–FIRE8: immediate 100 ms pulse on press;
- held button: another 100 ms pulse every 1,000 ms;
- all eight buttons: FIRE1–FIRE9 together for 500 ms once, release to re-arm;
- Button 1 + Button 8: FIRE9/Head Poof while held, maximum 10 seconds;
- all-buttons behavior has priority over normal repeats and Head Poof.

FIRE pins:

```text
GPIO8, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, GPIO21, GPIO47
```

## Buttons and USB

Buttons are active-HIGH with external 10k pull-downs:

```text
GPIO4, GPIO5, GPIO6, GPIO7, GPIO15, GPIO16, GPIO17, GPIO18
released = LOW
pressed  = HIGH / 3.3V
```

GPIO19/GPIO20 are native USB D-/D+ on both boards. USB CDC On Boot is
mandatory. Do not move diagnostics back to UART0.

## Live Build Settings

```cpp
ENABLE_REAL_ECLAIR_OUTPUT = true
FIRE_OUTPUTS_ENABLED = true
USE_INTERNAL_PULLDOWNS = false
```

OLED code and libraries are removed, not merely compiled out.

The boot mode is automatic LED animation. After Serial and link initialization,
a blocking five-second moving hardware check is requested from Eclair before
Wi-Fi. It uses temporary visible brightness and fixed nonzero speed without
modifying saved settings, then immediately resumes normal saved rendering. No
command or setup button is required for normal operation.

## FastLED Rules

Required FastLED release:

```text
3.9.20
```

Eclair must use Arduino-ESP32 2.0.17 / IDF4 so the FastLED 3.9.20 RMT4 worker
pool can schedule seven registered controllers over four ESP32-S3 TX channels.
Tardi remains on Arduino-ESP32 3.3.10 and must not drive local LED lanes.

Do not use FastLED 3.10.4 for Eclair7. Its forced RMT4 path does not build
correctly against Arduino-ESP32 3.3.10/IDF5, and 3.9.20 is the verified IDF4
seven-controller build.

The Eclair LED frame starts black. A 500 ms link timeout forces it black until
a valid state packet returns. Status and first-show diagnostics do not prove
physical light output.

FastLED ESP32 logging and full error handling are enabled during hardware
validation. Keep `firstShowAttempted` distinct from success, and do not add a
`transmissionSuccessful` status without a genuine driver result. Verbose
logging may be disabled only after physical validation or behind an explicit
diagnostic build flag.

## LED Architecture

Keep `firmware/esp32_controller/esp32_controller.ino` as coordinator.

The LED engine must not read physical buttons or touch FIRE pins. Tardi owns
debounce, accepted triggers, FIRE state, Head Poof, safety cutoffs, Serial,
settings, and web integration. Eclair receives complete CRC-checked state
snapshots, renders the engine, and owns physical LED output only.

LED active state uses accepted trigger windows:

```text
zoneActive = now < ledActiveUntil[zone]
```

Do not revert to held-button Pixelblaze LED state.

## Retired and Archival Code

`PBDriverAdapter` remains vendored as `PBDriverAdapter.cpp.reference` so the
Arduino builder does not compile it. It is not in the active output path. Do
not restore its `.cpp` extension or re-enable it unless explicitly requested.

The web UI source is `firmware/esp32_controller/web_setup_page.html`; firmware
serves its deterministic gzip representation from `web_setup_page_gzip.h`.
Keep them synchronized when changing the UI. Live values must continue to load
through `/api/status`.

Archival Pixelblaze material currently lives under:

```text
older files/reference_only/
```

Treat it as read-only. Use it only for layout and animation reference. Never
copy its GPIOs, FIRE logic/polarity, generated code, or old output settings
into live firmware.

## Editing Rules

- Preserve FIRE/button behavior unless the task explicitly changes it.
- Preserve native USB, Tardi UART1 ownership, and Eclair LED pin ownership.
- Prefer small targeted changes.
- Keep current-facing docs aligned with firmware; archive obsolete hardware notes.
- Do not claim software diagnostics prove physical LED wiring or power.

## vexp <!-- vexp v2.0.25 -->

**MANDATORY: use `run_pipeline` - do NOT grep or glob the codebase.**
vexp returns pre-indexed, graph-ranked context in a single call.

### Workflow
1. `run_pipeline` with your task description - ALWAYS FIRST
2. Make targeted changes based on the context returned
3. `run_pipeline` again only if you need more context

### Available MCP tools
- `run_pipeline` - primary context and impact tool
- `get_skeleton` - compact file structure
- `index_status` - indexing status

### Agentic search
- Do not use built-in file search, grep, or codebase indexing before `run_pipeline`
- If you spawn sub-agents or background tasks, pass them the context from `run_pipeline`

<!-- /vexp -->
