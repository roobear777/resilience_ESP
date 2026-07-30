# LED Twin production output

The production system uses two ESP32-S3 boards and one shared 1,908-pixel
logical animation engine.

| Owner | Zone | GPIO | Pixels |
|---|---|---:|---:|
| Tardi | Z1 Mouth | 1 | 208 |
| Tardi | Z2 Shoulder | 2 | 325 |
| Tardi | Z3 Midbody | 39 | 400 |
| Eclair | Z4 Rear | 4 | 300 |
| Eclair | Z5 Front legs | 5 | 300 |
| Eclair | Z6 Back legs | 6 | 300 |
| Eclair | Z7 Digestive | 7 | 75 |

Tardi uses its pinned FastLED LCD_CLOCKLESS implementation on Arduino-ESP32
3.3.10 for the three local lanes. Eclair uses FastLED 3.9.20 RMT4 on
Arduino-ESP32 2.0.17 for four lanes. Both use GRB order. Z3 remains a full
400-pixel local animation lane.

Tardi UART1 TX GPIO40 crosses to Eclair RX GPIO18. Tardi RX GPIO41 crosses from
Eclair TX GPIO17. The full-duplex link runs at 2,000,000 baud with common ground.
Tardi sends CRC-checked complete state every 20 ms; Eclair returns CRC-checked
status. Eclair renders from the shared engine and Tardi timebase. A 500 ms valid
state timeout forces all Eclair lanes black.

Startup begins black, initializes local output and link, then runs the existing
five-second temporary moving hardware check on both boards without saving
temporary settings. Normal saved rendering resumes immediately afterward.

Controller registration, first-show return, UART acknowledgement, and show
timing are separate software diagnostics. Physical LED output, colour order,
signal integrity, common ground, and power remain hardware tests.
