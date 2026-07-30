# LED Animation Architecture

## Pipeline

```text
accepted controller event
-> ledActiveUntil[zone]
-> zone renderer
-> HSV LedColor
-> palette/behavior/brightness settings
-> RGB CRGB frame
-> seven Eclair FastLED RMT4 channels (GRB wire order)
```

Tardi owns inputs, FIRE, accepted trigger events, web/settings, and runtime
output modes. It transmits state snapshots rather than pixels. Eclair owns the
LED engine, animation state, pixel rendering, and all physical LED GPIOs.

## State Model

Inactive zones render ambient animation continuously. Accepted press events set
a time window:

```text
zoneActive = now < ledActiveUntil[zone]
```

Active windows use the saved global animation duration, default 10 seconds.
They do not depend on a button remaining held.

## Logical Layout

| Zone | Pixels | Start | End |
|---:|---:|---:|---:|
| Z1 | 208 | 0 | 207 |
| Z2 | 325 | 208 | 532 |
| Z3 | 400 | 533 | 932 |
| Z4 | 300 | 933 | 1232 |
| Z5 | 300 | 1233 | 1532 |
| Z6 | 300 | 1533 | 1832 |
| Z7 | 75 | 1833 | 1907 |

Total: 1,908 logical animation pixels. Z8/button-station LEDs are absent from
the active engine.

Z3 uses its full eight-ring, 400-pixel geometry. All 400 pixels participate in
the existing Z3 renderer.

## Settings

Saved appearance includes master, ambient, active, global-look, and per-zone
brightness; saturation; speed; palette; behavior; and animation duration.

Brightness factors multiply. A zero factor can intentionally make part or all
of the saved look dark. The five-second startup hardware check temporarily
bypasses all saved brightness factors and saved speed, then normal rendering
resumes without modifying the settings. Web/Serial status reports a completely
dark saved ambient configuration.

## Output Modes

- `ANIMATION`: normal automatic operation and boot default
- `OFF`: black output
- `VALIDATE_SOLID`: fixed dim white
- `VALIDATE_CHANNEL`: one selected lane
- `VALIDATE_COLOR`: fixed red, green, or blue

Modes are runtime-only and are not restored after reboot.
