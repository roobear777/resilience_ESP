# GPIO Schema

This file is the compact GPIO schema for the live ESP32-S3 controller build.

## Board Target

```text
ESP32-S3-DevKitC-1-N8R8
ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module
USB Serial/debug baud: 115200
```

Normal uploads use the Arduino IDE Upload button. BOOT/RESET button handling is only a recovery step if upload stalls at `Connecting...`.

## Active Project Pins

| Function | GPIO | Direction | Electrical behaviour | Notes |
|---|---:|---|---|---|
| OLED SDA | GPIO1 | I2C | 3.3V I2C | SSD1306 SDA |
| OLED SCL | GPIO2 | I2C | 3.3V I2C | SSD1306 SCL |
| Button 1 | GPIO4 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 2 | GPIO5 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 3 | GPIO6 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 4 | GPIO7 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 5 | GPIO15 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 6 | GPIO16 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 7 | GPIO17 | input | LOW released, HIGH pressed | external 10k pull-down |
| Button 8 | GPIO18 | input | LOW released, HIGH pressed | external 10k pull-down |
| FIRE1 | GPIO8 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE2 | GPIO9 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE3 | GPIO10 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE4 | GPIO11 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE5 | GPIO12 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE6 | GPIO13 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE7 | GPIO14 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE8 | GPIO21 | output | HIGH idle, LOW trigger | active-LOW relay input |
| FIRE9 / Big Poof | GPIO47 | output | HIGH idle, LOW trigger | active-LOW relay input |
| Output Expander TX | GPIO39 | UART TX | UART idle HIGH | 2,000,000 baud |

## Button Electrical Contract

```text
3.3V -> button -> ESP32 GPIO input
GPIO input -> 10k pull-down -> GND
```

Released buttons read LOW. Pressed buttons read HIGH. Do not use 5V logic on ESP32 inputs.

## FIRE Electrical Contract

FIRE outputs are logic signals into external relay/input hardware. The ESP32 does not directly power solenoids or poofer loads.

```text
HIGH = idle / relay inactive
LOW  = trigger / relay active
```

Normal FIRE pulse:

```text
500 ms
```

Big Poof FIRE cutoff:

```text
10 seconds maximum while Button 1 + Button 8 remain held
```

## LED Output Expander Contract

```text
GPIO39 TX -> Output Expander DAT / UART input
ESP32 GND -> Output Expander GND
CLK unused for WS2811/WS2812-style LEDs
```

GPIO39 is a plausible ESP32-S3 UART TX pin and remains the current plan. It is JTAG-related, so reset/default-state behaviour is a physical hardware caveat. Firmware sets it to UART-idle HIGH after setup begins.

Fallbacks if GPIO39 physically fails:

```text
GPIO40
GPIO41
GPIO38 only after a new review
```

## Avoid / Reserved Pins

| Pin | Reason |
|---:|---|
| GPIO0 | boot/download related |
| GPIO14 | already FIRE7 |
| GPIO16 | already Button 6 |
| GPIO17 | already Button 7 |
| GPIO19/GPIO20 | USB-related caveats |
| GPIO23 | old PBDriverAdapter default, not Tardi wiring |
| GPIO35/GPIO36/GPIO37 | avoid on N8R8/octal PSRAM path |
| GPIO45/GPIO46 | strap pins |
| GPIO48 | onboard RGB/status LED related |
| GPIO1/GPIO2 | OLED |
