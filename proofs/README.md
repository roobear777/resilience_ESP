# LED Twin ordinary FastLED/RMT hardware proofs

These two standalone sketches are the first physical LED Twin proof. They test
the intended 3+4 LED-zone split, representative Tardi Wi-Fi/web load, and a
small matched UART command/acknowledgement link. They do not replace the
production firmware and do not implement the complete production LED Twin
animation engine or final communication protocol.

## First configuration to flash

- Board: ESP32-S3-DevKitC-1 with ESP32-S3-WROOM-1-N8R8
- Arduino-ESP32 core: **3.3.10**
- FastLED: **3.10.4**
- USB CDC On Boot: **Enabled**
- LED chipset/order: **WS2812B / GRB**
- API: ordinary `FastLED.addLeds()`
- Backend: RMT4 forced by `FASTLED_RMT5=0` in both sketches
- RMT workers: 4
- RMT memory blocks per worker: 1
- LED data path: ESP32-S3 GPIO directly to LED DIN
- Common ground: Tardi, Éclair, and both LED power supplies

Each sketch folder contains a `build_opt.h` whose complete contents are:

```text
-DESP32_ARDUINO_NO_RGB_BUILTIN=1
```

That compiler option is required globally when FastLED 3.10.4 uses legacy RMT4
with Arduino-ESP32 3.x. Do not replace it with only an in-sketch definition.

Do not add LCD/I80, GPIO0 padding, RMT5, I2S, PARLIO, SPI LED output, or a
Pixelblaze Output Expander to these proofs.

## Sketches and zone split

### Tardi: `led_twin_tardi_rmt_proof`

- Z1 Mouth: GPIO1, 208 pixels
- Z2 Shoulder: GPIO2, 325 pixels
- Z3 Midbody: GPIO39, 400 pixels
- UART TX: GPIO40
- UART RX: GPIO41
- Wi-Fi AP: `TARDI-TWIN-PROOF`
- Password: `tardiproof`
- Status page: `http://192.168.4.1`

The page polls `/status` ten times per second to create representative
AP/web-server load. The real button pins are inputs. All nine FIRE outputs are
initialized HIGH and are repeatedly held HIGH; this proof never activates one.

### Éclair: `led_twin_eclair_rmt_proof`

- Z4 Rear: GPIO4, 300 pixels
- Z5 Front legs: GPIO5, 300 pixels
- Z6 Back legs: GPIO6, 300 pixels
- Z7 Digestive: GPIO7, 75 pixels
- UART TX: GPIO17
- UART RX: GPIO18

Without a fresh Tardi command, Éclair continues using its defined local pattern
sequence so its four LED outputs can be tested independently.

## Matched proof UART

The two sketches use the same 2,000,000-baud, 8N1 line protocol:

```text
Tardi GPIO40 TX -> Éclair GPIO18 RX
Tardi GPIO41 RX <- Éclair GPIO17 TX
Tardi, Éclair, and LED supplies share ground

Command:         F,<frame number>,<pattern mode>
Acknowledgement: A,<frame number>
```

This is only the hardware-proof protocol, not the final production protocol.

## Physical test order

1. With power off, connect each board to only its listed LED data lanes and
   connect all grounds. Do not connect an LED lane or UART signal to a FIRE or
   button GPIO.
2. Flash Éclair with USB CDC On Boot enabled. Open native USB Serial at 115200.
   Confirm controller-registration messages and that the first
   `FastLED.show()` returns; then manually observe the local patterns on Z4-Z7.
3. Flash Tardi with USB CDC On Boot enabled. Before connecting effects hardware,
   confirm FIRE1-FIRE9 remain electrically HIGH/idle. Open native USB Serial at
   115200 and manually observe Z1-Z3.
4. Power off, cross-connect the two UART signals exactly as shown above, retain
   common ground, then power both boards.
5. Confirm Éclair reports Tardi-sourced frames and acknowledgements, while
   Tardi's acknowledgement count increases.
6. Connect a phone or laptop to the proof AP, leave the status page open, and
   monitor measured `FastLED.show()` timing and UART counts on both boards.
7. Manually verify all 1,908 physical pixels, GRB colour order, visible pattern
   correspondence, stable 30 Hz attempts, UART reliability, and FIRE idle
   voltage under the intended power and wiring conditions.

Software controller registration, a returned `FastLED.show()`, measured show
duration, and UART counters are separate diagnostics. None proves physical LED
visibility, signal integrity, wiring, power delivery, or FIRE voltage; those
remain hardware observations.

## Compile-check result

Both sketches were compile-checked with Arduino CLI 1.5.1, ESP32S3 Dev Module,
Arduino-ESP32 3.3.10, FastLED 3.10.4, Hardware CDC, USB CDC On Boot enabled,
8 MB flash, and OPI PSRAM. Both builds currently fail at link time with the
same missing FastLED RMT4 factory symbol:

```text
undefined reference to `fl::ChannelEngineRMT4::create()`
```

The sketch-local `FASTLED_RMT5=0` selects FastLED's RMT4 declarations, while
FastLED's separately compiled Arduino unity-build translation unit still uses
its ESP-IDF 5 default. A confirmation build that forced `FASTLED_RMT5=0`
globally reached FastLED's RMT4 implementation but failed inside FastLED 3.10.4
because that implementation does not compile against the installed ESP-IDF 5
headers (`canHandle`/`getCapabilities` interface mismatch and missing
`RMTMEM`). The required `build_opt.h` files remain exactly as specified; no
RMT5, alternate core, patched library, or alternate LED backend was silently
substituted.
