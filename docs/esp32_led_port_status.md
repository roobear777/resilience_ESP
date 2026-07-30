# Eclair7 Port Status

## Implemented

- Tardi retains all eight buttons, nine FIRE outputs, web UI, saved settings,
  Serial commands, accepted triggers, Head Poof, and safety cutoffs.
- Tardi sends complete versioned CRC-checked LED state over UART1 GPIO40/41.
- Eclair renders the shared engine and drives seven RMT4 lanes on GPIO4-10.
- Layout remains Z1-Z7, 1,908 pixels, including full 400-pixel Z3.
- The reverse UART path reports acknowledgements and output diagnostics.
- A 500 ms link timeout forces all Eclair outputs black.
- Native USB CDC remains on GPIO19/GPIO20 on both boards.
- OLED, Z8, GPIO40 setup-button ownership, and Pixelblaze output stay retired.

## Verified builds

```text
Tardi:  Arduino-ESP32 3.3.10, 950,314 / 3,342,336 bytes flash
        47,168 / 327,680 bytes static RAM
Eclair: Arduino-ESP32 2.0.17, pinned FastLED 3.10.4,
        748,177 / 1,310,720 bytes flash
        32,500 / 327,680 bytes static RAM
```

Both were compiled for ESP32-S3 Dev Module, 8 MB flash, OPI PSRAM, Hardware CDC,
and USB CDC On Boot. Physical LED output and the direct 3.3 V-to-DIN electrical
assumption still require validation on the finished wiring.
