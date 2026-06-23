# Tardi Controller

ESP32-S3 firmware for the Tardi sculpture controller.

This repo currently builds the live sculpture firmware:

- FIRE outputs are enabled.
- Real LED output through the Pixelblaze Output Expander is enabled.
- Ambient LEDs start automatically after boot.
- The `TARDI-LED` Wi-Fi controller is available while powered.
- FIRE outputs still use the 500 ms pulse and Big Poof cutoff safeguards.

## Upload

Use Arduino IDE with:

```text
Board: ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Board option: ESP32S3 Dev Module
USB Serial baud: 115200
```

Required Arduino libraries:

```text
Adafruit SSD1306
Adafruit GFX Library
```

Use the Arduino IDE Upload arrow normally. Holding BOOT or pressing RESET is not routine; use BOOT/RESET only if upload stalls at `Connecting...`.

## Web Controller

The ESP32 starts its Wi-Fi controller automatically:

```text
Wi-Fi:    TARDI-LED
Password: tardigrade
Open:     http://192.168.4.1
```

The web page controls LED look only: brightness, colour intensity, speed, palette, behaviour, ambient/animation target, whole/zone target, and animation duration.

Press `SAVE` to persist changes. `RESET` restores defaults in RAM until saved.

The web page does not control FIRE outputs.

## Normal Operation

On boot:

- ambient LED animation starts
- FIRE outputs idle HIGH
- buttons are ready
- Wi-Fi controller is available

Buttons 1-7 trigger their matching FIRE outputs and LED zones.

Button 8 triggers FIRE8, but does not directly start an independent LED zone.

Button 1 + Button 8 triggers Big Poof / FIRE9 and activates all LED zones together.

LED active windows use the saved global animation duration. Default is 10 seconds. When a zone's duration ends, it returns to ambient.

Normal FIRE outputs pulse for 500 ms. Big Poof / FIRE9 has a 10-second FIRE safety cutoff. LED animation duration does not change either FIRE timing.

## Essential Wiring Facts

- Button inputs are active-HIGH and use external 10k pulldowns.
- FIRE outputs are active-LOW: HIGH idle, LOW trigger.
- Output Expander TX is GPIO39 at 2,000,000 baud.
- ESP32 GPIO39 TX connects to Output Expander DAT.
- ESP32 GND must connect to Output Expander GND.
- Do not connect ESP32 GPIO pins to 5V logic.

Detailed pin and channel maps live in `docs/`.

## Optional Serial Checks

USB Serial uses `115200`.

Useful commands:

```text
led status
led settings
wifi status
```

Manual LED test commands still exist, but normal sculpture operation does not require typing `led animation`.

## Docs

- `docs/current_baseline.md` - current live-build baseline
- `docs/gpio_schema.md` - GPIO table
- `docs/pin_mapping.md` - pin rules and reservations
- `docs/interaction_logic.md` - button, FIRE, LED, and Big Poof behaviour
- `docs/esp32_led_port_status.md` - LED port status
- `docs/led_output_expander.md` - Output Expander wiring/channel reference
- `docs/led_animation_architecture.md` - LED render architecture
- `docs/web_setup_interface.md` - web controller notes

Historical working notes are in `docs/archive/`.
