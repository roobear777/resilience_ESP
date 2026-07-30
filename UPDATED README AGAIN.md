# Tardi LED Twin controller

Production LED Twin uses two ESP32-S3-DevKitC-1/WROOM-1-N8R8 boards. Tardi
retains every button, FIRE output, cutoff, combo, web endpoint, Wi-Fi AP, saved
setting, and interaction decision. Only physical LED output is split.

- Tardi: Arduino-ESP32 3.3.10, native USB, Z1-Z3 on GPIO1/2/39.
- Eclair: Arduino-ESP32 2.0.17, FastLED 3.9.20 RMT4, Z4-Z7 on GPIO4/5/6/7.
- UART1: Tardi TX40 -> Eclair RX18; Tardi RX41 <- Eclair TX17; 2,000,000 baud.
- Pixels: 1,908 total, WS2812B/GRB; Z3 remains 400 pixels.

Tardi sends CRC-checked complete LED-state snapshots. Eclair runs synchronized
copies of the production engine and returns CRC-checked status. Eclair starts
black and returns to black after a 500 ms link timeout.

Build and wiring details are in `docs/direct_led_output.md` and
`firmware/eclair_controller/README.md`. Hardware visibility, signal integrity,
power, and wiring still require testing on both boards.

## Build audit summary

The production audit checked pin ownership, FIRE/button isolation, packet
framing and CRC handling, startup and timeout behavior, source synchronization,
web status wording, and both compiler/library combinations. The coordinator
diff does not alter button debounce, accepted interactions, FIRE polarity,
pulse timing, repeat timing, combo priority, or cutoffs.

Resolved build issues and explicit operating assumptions are recorded in
`firmware/eclair_controller/README.md`. The important remaining validation item
is physical testing: a successful compile, an RMT/LCD driver symbol, or a
first-show diagnostic cannot prove that a powered LED strip emitted light.
