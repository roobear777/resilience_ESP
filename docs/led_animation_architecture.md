# LED Animation Architecture

The ESP32 now has a software-side logical render path for Z1-Z8 based on the
Pixelblaze source structure. The remaining work is validation and tuning, not a
fresh port of the zone render structure.

Remaining validation caveats:
- animation timing and feel still need visual validation
- colour order still needs physical LED validation
- real channel geometry still needs California Output Expander validation

## Rendering model
- ESP32 owns animation logic.
- Output Expander only outputs pixel data.
- Pixelblaze code is used as animation reference, not runtime code.

The animation layer should not call `FastLED.show()` or drive LED GPIOs directly when using the expander; it should pass RGB values to the Output Expander driver, currently expected to be `PBDriverAdapter`.

Current ESP32 implementation renders logical pixels as HSV `LedColor`, converts to normal RGB, then packs GRB callback bytes for the Pixelblaze Output Expander path. The runtime simulator exercises this packed-pixel callback path without calling `PBDriverAdapter::show()`.

## Frame loop
1. Read controller state.
2. Update LED mode/timers.
3. Render each zone pixel-by-pixel through `ledEngineRenderPixel(logicalPixelIndex, nowMs)`.
4. Convert HSV to RGB and pack callback bytes as GRB.
5. Future real output sends callback bytes through `PBDriverAdapter::show()`.
6. Expander latches channels together.

## LED modes
- Idle
- Local/single-button glow
- Full peristaltic wave
- Fade/cooldown
- Zone 7 always-on digestive pulse

## Zone render functions
- One render function per biological zone.
- Each function receives local pixel index, current time, and mode.
- Each zone function returns internal HSV `LedColor`; output conversion happens after logical rendering.

## Pattern source material
- Use old Pixelblaze patterns as references.
- Port timing/math/geometry ideas into C++.
- Do not depend on Pixelblaze web editor or JS runtime.

## Current wave timing
- Include the t=0, 150, 300, etc. sequence.

## Current zone geometry
- Zone 1: 208 LEDs
- Zone 2: 325 LEDs
- Zone 3: 400 LEDs
- Zone 4: assume current mapping
- Zone 5: 300 LEDs
- Zone 6: 300 LEDs
- Zone 7: 75 logical pixels until confirmed

## What not to import wholesale
- Pixelblaze UI sliders
- Pixelblaze-specific export syntax
- Pixelblaze GPIO/fire logic
- Old snippets that conflict with current mapping
