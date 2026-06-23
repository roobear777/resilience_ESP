# LED Animation Architecture

The ESP32 firmware now contains the software-side LED render path for Z1-Z8. The Pixelblaze source is reference material only; it is not runtime code.

## Rendering Model

```text
logical pixel index
-> zone lookup
-> zone renderer
-> LedColor HSV
-> LED settings scaling
-> LedRgbColor RGB
-> Output Expander callback
```

The LED engine does not read physical buttons directly and does not touch FIRE pins. The controller owns accepted button/FIRE events and calls LED trigger helpers.

## Ambient and Active State

Ambient rendering is continuous for inactive zones.

Active rendering is controlled by trigger windows:

```text
zoneActive = now < ledActiveUntil[zone]
```

Do not copy the Pixelblaze held-button model into ESP32 firmware.

## Trigger Mapping

```text
Button 1 -> Z1 / Mouth
Button 2 -> Z2 / Shoulder
Button 3 -> Z3 / Midbody
Button 4 -> Z4 / Rear
Button 5 -> Z5 / Front legs
Button 6 -> Z6 / Back legs
Button 7 -> Z7 / Digestive
```

Button 8 alone does not trigger an independent LED zone.

Z8 is the button-station LED zone. It mirrors/summarizes the Z1-Z7 station states and is also activated by the synchronized all-zone Big Poof LED event.

## Animation Duration

Normal zone triggers and Big Poof all-zone LED animation use the same saved global animation duration.

Default:

```text
10 seconds
```

After the duration expires, zones return to ambient rendering.

This LED duration does not change the 500 ms FIRE pulse or the 10 second Big Poof FIRE cutoff.

## LED Settings

Current saved LED settings include:

- master brightness
- colour intensity / saturation
- speed
- palette
- behaviour
- ambient level
- active level
- per-zone values
- global animation duration

The web controller can edit these settings. Live changes apply in RAM; `SAVE` persists them.

## Logical Layout

| Zone | Physical LEDs | Pixels | Logical start |
|---|---|---:|---:|
| Z1 | Mouth LEDs | 208 | 0 |
| Z2 | Shoulder LEDs | 325 | 208 |
| Z3 | Midbody LEDs | 400 | 533 |
| Z4 | Rear LEDs | 300 | 933 |
| Z5 | Front leg LEDs | 300 | 1233 |
| Z6 | Back leg LEDs | 300 | 1533 |
| Z7 | Digestive LEDs | 75 | 1833 |
| Z8 | Button station LEDs | 100 | 1908 |

Total logical pixels:

```text
2008
```

## Output

The live output path uses:

```text
ESP32-S3 -> PBDriverAdapter -> Pixelblaze Output Expander -> LED channels
```

Current PBChannel colour metadata is RGB:

```text
redi   = 0
greeni = 1
bluei  = 2
```

## Remaining Physical Checks

These are hardware/deployment checks, not missing animation-architecture pieces:

- final animation timing and feel on the sculpture
- physical channel order
- real LED colour appearance under sculpture power
- power injection and grounding under load
- Z7 physical wiring versus the current 75 logical-pixel model
