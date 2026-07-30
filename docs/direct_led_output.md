# Eclair7 LED Output

Tardi no longer renders or transmits physical LED pixels. It owns inputs, FIRE,
web/settings, accepted trigger windows, overrides, and runtime output modes. A
CRC-checked versioned snapshot of that complete LED state is sent every 20 ms
over UART1 to Eclair. Eclair applies the snapshot, renders the shared production
animation engine, and drives all seven physical zones.

The full-duplex wiring is:

```text
Tardi GPIO40 TX -> Eclair GPIO18 RX
Tardi GPIO41 RX <- Eclair GPIO17 TX
common ground
```

Eclair returns acknowledgement, link state, frame count, last show time, CRC
errors, timeout count, and first-show-attempted status. A snapshot is complete,
so reconnect and reboot do not depend on missed incremental events. At 500 ms
without a valid packet, Eclair sends black and stays black until communication
recovers.

## Physical lanes

| Lane | Zone | Eclair GPIO | Pixels | Frame start |
|---:|---|---:|---:|---:|
| 1 | Z1 Mouth | 4 | 208 | 0 |
| 2 | Z2 Shoulder | 5 | 325 | 208 |
| 3 | Z3 Midbody | 6 | 400 | 533 |
| 4 | Z4 Rear | 7 | 300 | 933 |
| 5 | Z5 Front legs | 8 | 300 | 1233 |
| 6 | Z6 Back legs | 9 | 300 | 1533 |
| 7 | Z7 Digestive | 10 | 75 | 1833 |

Total: 1,908 pixels, GRB order. The selected electrical path is direct Eclair
GPIO to 5 V WS2812-class DIN, with common ground and no buffer, level shifter,
or series data resistor.

## FastLED toolchain

Eclair uses FastLED 3.9.20 with Arduino-ESP32 2.0.17 / IDF 4.4.7. That
combination selects FastLED RMT4 and schedules seven registered controllers
over the ESP32-S3's four TX workers. Arduino-ESP32 3.3.10 / IDF5 with FastLED
3.10.4 cannot compile the forced RMT4 implementation. Tardi remains on core
3.3.10 and does not link FastLED.

The five-second startup check remains Tardi-controlled: it sets a temporary
startup-test flag in outgoing state without changing saved settings. The test
can only light LEDs when Eclair is powered, wired, and receiving valid packets.
Software compilation and status do not prove physical wiring, power, or light.
