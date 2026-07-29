# GPIO Schema

Exact pin ownership for the live ESP32-S3 build.

## Direct LED Data

| Zone | GPIO | Pixels |
|---:|---:|---:|
| Z1 Mouth | 1 | 208 |
| Z2 Shoulder | 2 | 325 |
| Z3 Midbody | 39 | 400 |
| Z4 Rear | 40 | 300 |
| Z5 Front legs | 41 | 300 |
| Z6 Back legs | 42 | 300 |
| Z7 Digestive | 43 | 75 |

GPIO0 is an internal `LCD_CLOCKLESS` dummy/padding pin and must remain
unwired. GPIO19/GPIO20 are native USB D-/D+.

## Buttons

| Button | GPIO |
|---:|---:|
| 1 | 4 |
| 2 | 5 |
| 3 | 6 |
| 4 | 7 |
| 5 | 15 |
| 6 | 16 |
| 7 | 17 |
| 8 | 18 |

Inputs are active-HIGH:

```text
released = LOW through external 10k pull-down
pressed  = HIGH / 3.3V
```

## FIRE

| Output | GPIO |
|---:|---:|
| FIRE1 | 8 |
| FIRE2 | 9 |
| FIRE3 | 10 |
| FIRE4 | 11 |
| FIRE5 | 12 |
| FIRE6 | 13 |
| FIRE7 | 14 |
| FIRE8 | 21 |
| FIRE9 / Head Poof | 47 |

```text
HIGH = idle
LOW  = triggered
```

FIRE pins are logic outputs into external relay/input hardware. They do not
power loads directly.

## Reserved and Retired

| Pin/resource | Rule |
|---|---|
| GPIO0 | Internal FastLED ownership; no external connection |
| GPIO19/GPIO20 | Native USB |
| GPIO45/GPIO46 | Strap pins; avoid |
| GPIO48 | Onboard RGB/status LED; avoid |
| GPIO1/GPIO2 | Direct LEDs; no OLED |
| GPIO39 | Direct Z3 data; no Output Expander UART |
| GPIO40 | Direct Z4 data; no setup button |
| UART0 | Do not use; conflicts with GPIO43/Z7 |

Never apply 5 V directly to an ESP32 GPIO.
