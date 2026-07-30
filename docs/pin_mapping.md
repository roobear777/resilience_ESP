# Pin Mapping Rules

The production Eclair7 architecture uses two ESP32-S3-DevKitC-1 boards with
ESP32-S3-WROOM-1-N8R8 modules. GPIO numbers on different boards do not conflict.

## Tardi ownership

- GPIO4/5/6/7/15/16/17/18: active-HIGH button inputs with external 10k pull-downs.
- GPIO8/9/10/11/12/13/14/21/47: active-LOW FIRE1-FIRE9 outputs.
- GPIO40: UART1 TX to Eclair GPIO18.
- GPIO41: UART1 RX from Eclair GPIO17.
- GPIO19/GPIO20: native USB D-/D+.

Tardi has no physical LED data outputs. OLED, Pixelblaze Output Expander, Z8,
and the former GPIO40 web-setup button remain absent.

## Eclair ownership

| Zone | GPIO | Pixels |
|---:|---:|---:|
| Z1 Mouth | 4 | 208 |
| Z2 Shoulder | 5 | 325 |
| Z3 Midbody | 6 | 400 |
| Z4 Rear | 7 | 300 |
| Z5 Front legs | 8 | 300 |
| Z6 Back legs | 9 | 300 |
| Z7 Digestive | 10 | 75 |

Eclair GPIO18 is UART1 RX from Tardi GPIO40. Eclair GPIO17 is UART1 TX to
Tardi GPIO41. GPIO19/GPIO20 remain native USB D-/D+.

## Native USB

Build both boards with Hardware CDC and USB CDC On Boot enabled. Both sketches
intentionally fail compilation when USB CDC On Boot is disabled.

## Selected LED electrical path

```text
Eclair ESP32-S3 GPIO -> 5 V WS2812-class LED DIN
```

There is no buffer, level shifter, or series data resistor in the selected
architecture. ESP32, Tardi, Eclair, and LED-power grounds are common. Never
apply 5 V to an ESP32 input or GPIO rail.

Avoid GPIO35/36/37 on N8R8 octal-PSRAM hardware, strap pins GPIO0/45/46, and
the likely onboard RGB/status pin GPIO48.
