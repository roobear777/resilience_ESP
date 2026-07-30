# LED Twin production firmware

Tardi remains the complete interaction controller: eight buttons, nine active-LOW
FIRE outputs and cutoffs, Wi-Fi/web UI, saved settings, combos, and accepted
LED triggers. Tardi renders and drives Z1-Z3. It sends complete CRC-checked LED
state snapshots to Eclair, which runs the synchronized production animation
engine and drives Z4-Z7. A 500 ms link timeout forces Eclair black until a valid
state packet returns.

## Wiring

```text
Tardi GPIO1  -> Z1 Mouth DIN, 208
Tardi GPIO2  -> Z2 Shoulder DIN, 325
Tardi GPIO39 -> Z3 Midbody DIN, 400

Tardi GPIO40 TX -> Eclair GPIO18 RX
Tardi GPIO41 RX <- Eclair GPIO17 TX
Common ground

Eclair GPIO4 -> Z4 Rear DIN, 300
Eclair GPIO5 -> Z5 Front legs DIN, 300
Eclair GPIO6 -> Z6 Back legs DIN, 300
Eclair GPIO7 -> Z7 Digestive DIN, 75
```

All zones are WS2812B/GRB. Native USB remains GPIO19/GPIO20 on both boards.

## Toolchains

| Target | Arduino-ESP32 | FastLED | Output |
|---|---:|---:|---|
| Tardi | 3.3.10 | pinned production revision | LCD_CLOCKLESS, three lanes |
| Eclair | 2.0.17 / IDF 4.4.7 | 3.9.20 | RMT4, four workers |

FastLED 3.10.4 must not be used for Eclair RMT4: its channel-driver refactor
does not compile when RMT4 is genuinely enabled. FastLED 3.9.20 was selected
because its ESP32-S3 binary contains `ESP32RMTController` under core 2.0.17.
Eclair's build options set four RMT channels and one memory block per channel.

Both targets use ESP32S3 Dev Module, 8 MB flash, OPI PSRAM, Hardware CDC, and
USB CDC On Boot enabled. The Eclair PlatformIO environment pins its core and
FastLED version.

## Audit: issues and resolutions

| Issue found or risk checked | Resolution in this build |
|---|---|
| Seven FastLED controllers cannot be scheduled by the four-channel ESP32-S3 RMT4 worker pool as one board | Physical output is split: Tardi registers exactly three LCD_CLOCKLESS lanes and Eclair registers exactly four RMT4 lanes. No dummy LED controller is registered. |
| FastLED 3.10.4 appeared to compile for Eclair only when it silently selected a blocking fallback; genuine RMT4 then failed inside that release | Eclair is pinned to FastLED 3.9.20 and Arduino-ESP32 2.0.17/IDF4. The resulting ELF contains `ESP32RMTController`/`RMTMEM` and does not contain `ClocklessBlockingGeneric`. |
| Tardi's production LCD_CLOCKLESS implementation requires the pinned FastLED revision | Tardi remains on Arduino-ESP32 3.3.10 and revision `fa79f3f757ca2dadd5db7773b2bed5c13b26b33a`; its compile include path was audited against that checkout. |
| A UART byte loss can misalign a packed stream | Both receivers use magic, protocol version, type, packet size, and CRC, then slide one byte at a time until framing is recovered. Invalid state never replaces the last valid state. |
| The original receive diagnostic counted every rejected sliding window as a CRC error | The counter now increments only when a candidate has the complete expected header but fails CRC. Framing recovery itself is not mislabeled as data corruption. |
| Wire arrays and enum ordinals could drift silently from the shared engine | Compile-time checks now bind the protocol to seven zones, two looks, output modes 0-4, and validation colors 0-2. A protocol-affecting change must deliberately update both targets and the version. |
| Eclair could retain stale light after a stopped or unplugged Tardi link | Eclair starts with an explicitly black show and forces all four lanes black 500 ms after the last valid state packet. A later valid packet automatically resumes rendering. |
| FIRE/button behavior could accidentally move into the LED engine | The Eclair sketch and shared LED engine contain no button reads or FIRE writes. The Tardi coordinator changes are limited to link startup/update and LED-output routing. FIRE remains active-LOW and Tardi-owned. |
| Current web wording could imply that a software show proves light output | Status distinguishes Tardi first-show attempt from Eclair link/status. Documentation consistently treats first-show and driver routing as diagnostics, not proof of physical light. |
| Arduino cannot compile shared sources from a sibling sketch folder | Tardi remains canonical; `sync_shared_led_sources.sh` refreshes Eclair's Arduino-local copies. The copies are compared before release builds. |

## Assumptions and known limits

- Both boards are ESP32-S3-DevKitC-1/WROOM-1-N8R8 and use 3.3 V UART logic.
  UART TX/RX are crossed, both boards and LED supplies share ground, and the
  2,000,000-baud wiring is short and electrically clean.
- LED power is provided and fused for the installed strips; it is not sourced
  from a DevKit pin. Direct 3.3 V LED data is assumed to meet the installed
  WS2812B input threshold. If hardware testing shows marginal data, add a
  suitable 3.3-to-5 V level shifter rather than changing firmware timing.
- The packed protocol is intentionally ESP32-to-ESP32: both ends are
  little-endian and use the same fixed-width field layout. CRC detects link
  corruption; it is not authentication or encryption.
- Tardi is the only settings authority and the only board that persists saved
  settings. Eclair uses received snapshots in RAM and may be reflashed or
  replaced without migrating settings.
- The complete five-second moving startup check reaches both boards only when
  Eclair is powered and linked during Tardi startup. If Eclair comes up later,
  it safely starts black and joins the current normal state, but it does not
  replay the missed startup check.
- State snapshots synchronize settings, accepted active-zone windows, runtime
  mode, overrides, and the Tardi timebase. If Eclair alone resets after a long
  run, it immediately recovers logical state, but stateful animation phases can
  restart at a different phase. Use a paired power cycle when strict visual
  phase alignment across Z1-Z7 is required. This does not affect FIRE safety or
  active-zone timing.
- Tardi deliberately continues Z1-Z3 and all interaction/FIRE authority if
  Eclair is offline. Eclair deliberately fails only Z4-Z7 to black.
- Hardware output, signal integrity, power distribution, thermal behavior, and
  the full 1,908-pixel load remain physical acceptance tests. Software alone
  cannot close those items.

## Protocol and safety

Tardi sends a packed state snapshot every 20 ms at 2,000,000 baud. It includes
sequence/time, active-zone mask, output/validation mode, all appearance
settings, pressure/startup/all-green overrides, and CRC-16. Eclair returns
CRC-checked status with acknowledgement sequence, frame/show timing, CRC error
count, timeout count, link state, and first-show state.

Eclair starts black. Invalid packets cannot change state. If no valid packet is
received for 500 ms, all four Eclair lanes are shown black. Software diagnostics
do not prove physical light, wiring, grounding, power, or signal integrity.

The state packet is 119 bytes (about 0.6 ms on an 8-N-1 2 Mbaud link), leaving
substantial transport margin at the 20 ms interval. Status is 38 bytes every
250 ms. Eclair renders at a nominal 33 ms interval. Sequence acknowledgement
is diagnostic; safety depends on receipt of a valid packet within 500 ms, not
on an acknowledgement round trip.

## Shared sources

Eclair keeps Arduino-local copies of the canonical Tardi LED engine because
Arduino does not compile sibling sketch folders. After changing the canonical
engine or protocol, run `sh sync_shared_led_sources.sh` here, then compile both
targets. Do not edit `src/shared` independently.

Verified compile sizes for this architecture:

```text
Tardi  (core 3.3.10, pinned FastLED): 1,288,135 bytes flash; 52,480 bytes static RAM
Eclair (core 2.0.17, FastLED 3.9.20 RMT4): 333,529 bytes flash; 26,324 bytes static RAM
```
