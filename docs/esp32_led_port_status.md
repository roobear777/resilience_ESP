# ESP32 LED Twin port status

- Tardi production compiles on Arduino-ESP32 3.3.10 and drives three
  LCD_CLOCKLESS lanes: GPIO1, GPIO2, GPIO39.
- Eclair production compiles on Arduino-ESP32 2.0.17 with FastLED 3.9.20 and
  contains the ESP32 RMT4 controller for GPIO4-GPIO7.
- UART1 state/status transport is CRC-checked at 2,000,000 baud.
- Eclair starts black and a 500 ms state timeout forces black.
- Native USB CDC remains required on both boards.
- Software builds and diagnostics do not prove physical LED output.

See `docs/direct_led_output.md` for the complete production map and
`firmware/eclair_controller/README.md` for exact build settings.
