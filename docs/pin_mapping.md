# Pin Mapping Rules

The compact live table is in `docs/gpio_schema.md`.

## Ownership

- GPIO1/2/39/40/41/42/43 belong to direct LED lanes Z1–Z7.
- GPIO0 belongs internally to `LCD_CLOCKLESS` and remains unwired.
- GPIO19/GPIO20 belong to native USB.
- GPIO4/5/6/7/15/16/17/18 are active-HIGH button inputs.
- GPIO8/9/10/11/12/13/14/21/47 are active-LOW FIRE outputs.

Do not restore OLED, Output Expander UART, or the old GPIO40 setup button
without an explicit pin-ownership redesign.

## Native USB

GPIO43 is Z7 data, so UART0 cannot be used for Serial. Build with:

```text
USB Mode: Hardware CDC and JTAG
USB CDC On Boot: Enabled
```

The firmware intentionally fails compilation when USB CDC On Boot is disabled.

## LED Electrical Rules

ESP32 LED GPIOs are 3.3 V signals. Feed zone DIN through the documented
SN74AHCT244 5 V logic buffer and 100 ohm series resistors. Never apply 5 V to
an ESP32 pin. All logic and LED-system grounds must be common.

## Caution Pins

| GPIO | Rule |
|---:|---|
| 0 | Internal LCD dummy; unwired; also a strap pin |
| 35/36/37 | Avoid on N8R8/octo-PSRAM hardware |
| 45/46 | Strap pins; avoid |
| 48 | Onboard RGB/status LED; avoid |

The board-labelled TX/RX pins are not substitutes for native USB.
