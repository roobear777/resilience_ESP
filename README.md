# Tardi Controller

ESP32-S3 firmware for the Resilience tardigrade sculpture.

> **Current firmware is [`EclairTardiSplit/`](EclairTardiSplit/).**
> Two boards, two sketches. Start with that README — it covers the
> architecture, the pin maps, and how the project got here.

## Architecture

The sculpture runs on **two independent ESP32-S3 boards** with no
communication link between them. The button wires reach both boards, so the
buttons themselves keep the two in sync.

| Board | Sketch | Job |
|---|---|---|
| **Tardi** | `EclairTardiSplit/Tardi_Fire/` | 8 buttons, FIRE1-9, OLED. No WiFi. |
| **Eclair** | `EclairTardiSplit/Eclair_LED/` | 8 buttons, Z1-Z8 animation, 8 WS2812 lanes, WiFi tuning |

LED output no longer uses a Pixelblaze Output Expander. Eclair drives eight
parallel WS2812 lanes directly from GPIO through FastLED's `LCD_CLOCKLESS`
driver, into an SN74AHCT244 buffer.

## Quick start

1. Read [`EclairTardiSplit/README.md`](EclairTardiSplit/README.md).
2. On new hardware, flash `EclairTardiSplit/LCD_Driver_Test/` first. It
   verifies the driver, the pin map and the DMA allocation, and is safe to run
   with no LEDs attached.
3. Then flash the two real sketches.

Arduino IDE, both boards:

```text
Board:        ESP32S3 Dev Module
Module:       ESP32-S3-WROOM-1 (N8R8)
PSRAM:        OPI PSRAM
Flash Size:   8MB
Serial baud:  115200
```

**Eclair also needs `USB CDC On Boot: ENABLED`** and the **USB** port, not the
UART port — GPIO43 is LED lane Z7, so UART0 is gone on that board.

Libraries: `Adafruit SSD1306`, `Adafruit GFX` (Tardi), `FastLED` (Eclair).

## Web controller

Runs on **Eclair only**. Never on the fire board.

```text
Wi-Fi:    TARDI-LED
Password: tardigrade
Open:     http://192.168.4.1
```

LED look only — brightness, colour intensity, speed, palette, behaviour,
ambient/animation target, zone target, animation duration. `SAVE` persists.
It does not control FIRE.

## Behaviour

| Input | Tardi | Eclair |
|---|---|---|
| Button 1-7 | FIRE1-7 pulse while held | triggers Z1-Z7 |
| Button 8 | FIRE8 | no zone of its own |
| B1 + B8 | FIRE9 head poof, 10 s cutoff | full-body: all zones |
| B2 + B6 | — | everything green |
| All 8 | one coordinated 500 ms pulse | — |

LED windows use the saved animation duration (default 10 s), then return to
ambient. A press restarts the window rather than being ignored.

**FIRE pulse timing:** normal pulses are **100 ms**, repeating every 1000 ms
while held. The all-8 pulse is 500 ms. Head Poof has a 10 s cutoff.

> Older revisions of this README and `docs/current_baseline.md` claimed 500 ms
> for normal pulses. That was never what the code did.

## Essential wiring facts

- Button inputs are active-HIGH with external 10k pull-downs, and the **same
  wire reaches both boards on the identically-numbered GPIO**.
- FIRE outputs are active-LOW: HIGH idle, LOW trigger.
- LED data leaves Eclair at 3.3 V and must pass through an **SN74AHCT244** at
  5 V with 100 ohm series resistors before reaching any strip.
- **No LED current through any PCB.** Power comes off a bus bar straight to the
  strips. Routing LED power through a controller board is what destroyed the
  Output Expander.
- All grounds bond at **one** point. "Connected" is not the same as "at the
  same potential" when tens of amps are involved.
- Do not connect ESP32 GPIO pins to 5V logic.

## Docs

**Current:**

- [`EclairTardiSplit/README.md`](EclairTardiSplit/README.md) — architecture and history
- `docs/two_board_split.md` — split reference
- `docs/buffer_board.md` — SN74AHCT244 build sheet
- `docs/power_and_ground.md` — power topology, current budget, injection
- `docs/preflight_checklist.md` — staged bring-up with go/no-go gates
- `docs/enclosure_review.md` — assessment of the current build

**Still accurate:**

- `docs/gpio_schema.md`, `docs/pin_mapping.md` — pin rules and reservations
- `docs/interaction_logic.md` — button and fire interactions
- `docs/led_animation_architecture.md` — render architecture
- `docs/web_setup_interface.md` — web controller notes

**Superseded, kept for reference:**

- `docs/current_baseline.md` — single-board build; fire pulse figure is wrong
- `docs/esp32_led_port_status.md` — expander-era LED output
- `docs/led_output_expander.md` — the expander is no longer in the signal path
- `firmware/esp32_controller/` — single-board firmware
- `esp32_code_july_24/` — the snapshot both current sketches were split from
- `reference_only/Pixelblaze/` — original SOAK pattern code

Historical working notes are in `docs/archive/`.

## Outstanding

The software is proven; the remaining work is hardware.

1. **Power and ground** — before anything is flashed to live hardware.
2. **The SN74AHCT244 buffer board.** Until it exists, strips run at 3.3 V
   against a 3.5 V threshold and inconsistent behaviour is expected.
3. **Get the button pull-downs and OLED off breadboard.**

Two numbers for the power work:

- **Z7 is 525 physical LEDs, not 75** — seven parallel strands share one data
  line. Total physical is **2,358**, not 2,008.
- **Ambient draw is roughly 11 A** against a 15 A supply. One button press adds
  up to 12 A more.
