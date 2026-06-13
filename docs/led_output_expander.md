# LED Output Expander Notes

## Purpose

This document is the reference/README for the ESP32-S3 -> Pixelblaze Output Expander -> LED zones architecture.

It covers expander hardware, wiring, serial communication, output channel mapping, and bench tests. Animation logic belongs in `docs/led_animation_architecture.md`.

For current Tardi ESP32 integration status and handoff notes, see `docs/esp32_led_port_status.md`.

## Current hardware

- Standard Pixelblaze Output Expander v3.0, not Pro.
- 8 output channels, numbered 0–7.
- Input side has `GND / DAT / CLK / 5V`.
- Address jumpers are `JP1 / JP2 / JP3`.
- Current evidence indicates the standard expander supports the needed 300–400 RGB pixel zones.
- Pro is not required for pixel count.

## Current architecture assumption

ESP32-S3 runs:

- controller logic
- LED animation logic
- LED frame generation

The Output Expander handles:

- 8-channel LED output timing
- 5V level-shifted LED data
- simultaneous channel updates after `DRAW_ALL`

Target architecture:

```text
ESP32-S3 → UART → Pixelblaze Output Expander → LED zones
```

## ESP32 to expander wiring

```text
ESP32-S3 UART TX  →  Expander DAT
ESP32-S3 GND      →  Expander GND
5V supply         →  Expander / LED power as appropriate
```

Notes:

- `CLK` is unused for WS2811 / WS2812B.
- `CLK` is only relevant for APA102 / DotStar-style clocked LEDs.
- No level shifter is expected between ESP32-S3 UART TX and expander `DAT`.
- ESP32 TX-only is expected to be sufficient; expander RX back to ESP32 is not required for the basic protocol.
- Shared ground is required.

## Power notes

The current board is the standard expander, not the Pro.

Do not treat the standard expander as major power distribution hardware.

- Standard expander: useful as LED data output hardware.
- Pro expander: adds 15A fused power distribution.
- Current board does not have Pro-style fused distribution.
- Sculpture power injection, 5V spines, and ground distribution remain separate wiring concerns.

## Communication

- Protocol: Pixelblaze Output Expander serial protocol.
- Transport: UART serial.
- Baud rate: `2,000,000`.
- Arduino/C++ driver/library: `simap/PBDriverAdapter`.
- `PBDriverAdapter` is software, not hardware.
- It formats RGB pixel data into the serial frames expected by the Output Expander.
- ESP32 sends channel pixel data through `PBDriverAdapter`.
- Expander receives channel data, then `DRAW_ALL` updates outputs together.
- ESP32 code should generate RGB pixel values; the expander handles LED timing.

## Working output channel map

Use this as the current baseline unless Whit corrects it.

```text
Output 0 = Zone 8 string/stations / 100 pixels / logical start 1908
Output 1 = Zone 1 mouth / 208 pixels / logical start 0
Output 2 = Zone 2 shoulder / 325 pixels / logical start 208
Output 3 = Zone 3 midbody / 400 pixels / logical start 533
Output 4 = Zone 4 rear / 300 pixels / logical start 933
Output 5 = Zone 5 legs / first legs group / 300 pixels / logical start 1233
Output 6 = Zone 6 legs / second legs group / 300 pixels / logical start 1533
Output 7 = Zone 7 digestive / 75 logical pixels / logical start 1833
```

Do not assume Zone 1 maps to Output 0.

## Current LED assumptions

- Regular WS2812B strips use GRB colour order.
- Zones 3, 4, and 7 use 4-wire WS2811-style spool wiring.
- Output 7’s 75-pixel logical count is intentional for now, even though older docs mention 400 physical digestive LEDs.
- Treat Zone 7 as “75 logical pixels / parallel digestive runs” until corrected.

## Keep / discard rules

Keep as current:

- Standard Output Expander v3.0.
- ESP32-S3 drives expander over UART.
- Output map listed above.
- 300–400 pixel channels are acceptable for planning.
- Pro is only relevant if the installation needs fused power distribution.

Treat as old or secondary:

- Any 48-LED Zone 1 ambient snippet.
- Any provisional channel map that says outputs 0–4 are simply Zones 1–4 without the master pattern mapping.
- Any Pixelblaze button or FIRE/poofer logic in old patterns.

## Must confirm with Whit

- Actual live Pixelblaze Output Expander config.
- Physical output 0–7 wiring.
- LED type per output.
- Colour order per output.
- Whether Zone 7 is definitely 75 logical pixels / parallel physical runs.
- Whether any output uses APA102/DotStar requiring `CLK`.
- Current power injection / distribution scheme.

## Bench test plan

### Stage 1: one short strip

- Connect ESP32-S3 UART TX to expander `DAT`.
- Connect ESP32-S3 GND to expander GND.
- Power expander and test LED strip appropriately.
- Configure one output for 30 WS2812B pixels.
- Run simple red / green / blue / white / off cycle.
- Confirm expander status LEDs and strip output.

### Stage 2: all outputs

- Test outputs 0–7 one at a time.
- Use a unique colour per output.
- Confirm physical channel order.

### Stage 3: large channel

- Configure one output for 300–400 pixels.
- Run a chase or index-visible pattern.
- Confirm the full strip updates correctly.

### Stage 4: current channel map

Configure the working output map:

```text
0: 100
1: 208
2: 325
3: 400
4: 300
5: 300
6: 300
7: 75
```

Confirm all active channels update together.
