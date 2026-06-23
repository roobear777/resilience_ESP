# Tardi Controller Working Rules

## Critical Safety Rules

Do not change casually:

- FIRE GPIO pins
- FIRE active-LOW polarity
- button pins
- OLED pins
- Big Poof cutoff behaviour
- LED Output Expander channel mapping
- Output Expander UART TX pin / baud

FIRE outputs are active-LOW:

```text
idle      = HIGH
triggered = LOW
return    = HIGH
```

Normal FIRE1-FIRE8 pulse duration is 500 ms.

Big Poof / FIRE9 is Button 1 + Button 8 and retains the 10-second FIRE cutoff.

## Current Live Build State

The checked firmware is currently a live hardware build.

Expected live settings:

```cpp
ENABLE_REAL_PB_EXPANDER_OUTPUT = true
FIRE_OUTPUTS_ENABLED = true
USE_INTERNAL_PULLDOWNS = false
```

Ambient LED animation starts automatically after boot through the Output Expander path.

The web controller AP is available while powered.

Do not switch between live hardware and simulator/development behavior unless the task explicitly asks for it.

## Pin / Hardware Rules

Button inputs are active-HIGH:

```text
released = LOW
pressed  = HIGH / 3.3V
```

Live wiring uses external 10k pulldowns.

Output Expander UART:

```text
TX pin = GPIO39
baud   = 2000000
```

Do not use board-labelled TX/RX / UART0 for the Output Expander.
Do not use GPIO16 for Output Expander TX; it is Button 6.
Do not use PBDriverAdapter's old default ESP32 TX GPIO23 for Tardi real output.

## PixelBlaze Reference Rules

`reference_only/` is read-only.

Do not modify anything inside `reference_only/`.

Do not copy PixelBlaze GPIO numbers, FIRE logic, FIRE polarity, generated `dist/` code, or archived old pattern files into ESP32 firmware.

Use PixelBlaze source only for LED layout, animation structure, and per-zone behavior.

## LED Architecture Rules

Keep `firmware/esp32_controller/esp32_controller.ino` as the controller coordinator.

The LED engine must not read physical buttons directly.
The LED engine must not touch FIRE pins.

Controller logic owns:

- button debounce
- accepted button/FIRE triggers
- FIRE outputs
- Big Poof logic
- safety cutoffs
- OLED diagnostics
- Serial logging

LED active state uses accepted trigger windows:

```text
zoneActive = now < ledActiveUntil[zone]
```

Do not revert to PixelBlaze held-button active logic.

## Output Expander Rules

`PBDriverAdapter` is vendored under:

```text
firmware/esp32_controller/src/PBDriverAdapter/
```

Do not modify vendored PBDriverAdapter files unless explicitly requested.

Tardi local patch:

```cpp
void PBDriverAdapter::begin(uint32_t uartFrequency, int8_t txPin);
```

The physical output path is:

```text
ESP32-S3 -> UART GPIO39 -> Pixelblaze Output Expander -> LED zones
```

Current physical colour metadata is RGB:

```text
redi   = 0
greeni = 1
bluei  = 2
```

## Editing Rules

Do not modify `reference_only/`.

Do not add FastLED, NeoPixelBus, or Adafruit NeoPixel.

Do not change FIRE/button/OLED behavior while working on LED rendering or web UI unless explicitly requested.

Prefer small targeted edits that preserve existing behavior.

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
