
## Start Here

First read these files in order
Second when ready to connect the ESP come back and read below:


docs/current_baseline.md
docs/interaction_logic.md
docs/pin_mapping.md
docs/test_plan.md
docs/esp32_led_port_status.md
[California LED Output Expander Validation](VALIDATION_README.md)
firmware/esp32_controller/esp32_controller.ino

Arduino Serial Monitor is useful for manual setup checks, but close Serial Monitor / Serial Plotter before running the Python validation script. Only one program can use the ESP32 USB Serial port at a time.


## ESP32-S3 First Setup and UPLOAD, and UPLOAD thereafter (theres a difference)

Controller board:

ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module

Do not pick `ESP32S3 Dev Module Octal (WROOM2)` for the current controller board.


## Setup

1. Download and install Arduino IDE.
2. Connect the ESP32-S3 using the **left USB-C port** and a data-capable USB cable.
3. In Arduino IDE, select:


```text
Board: ESP32S3 Dev Module
Port: /dev/cu.usbserial-0001 (or correct port)
Serial baud: 115200
```




## First time connection only

1. Plug in using the **left USB-C port**.
2. Open Arduino IDE.
3. Open: File → Examples → 01.Basics → Blink
4. Replace the Blink sketch with the serial test sketch BELOW.
5. Press and hold **BOOT**.
6. While holding **BOOT**, click the **Upload** arrow.
7. Keep holding **BOOT** until Arduino starts writing to the board.
8. Release **BOOT** once writing begins.
9. Wait for upload to complete.
10. Press **RESET** once.
11. Open **Serial Monitor**.
12. Set baud rate to `115200`.
13. Confirm serial output shows `ESP32-S3 is alive` and repeated `tick` messages.

## Uploading AFTER first setup

1. Click the Arduino IDE **Upload** arrow.
2. Wait for Arduino output to show:

```text
Connecting...
```

3. Press and hold **BOOT**.
4. Release **BOOT** once writing begins.
5. Wait for upload to complete.
6. Sometimes you have to press  **RESET** if serial not showing
7. Open **Serial Monitor** at `115200` if serial output needs checking.

## Serial test sketch

Use this instead of the default Blink code.

Default Blink sketch may not work because onboard LED pin is different on this board.

```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 is alive");
}

void loop() {
  Serial.println("tick");
  delay(1000);
}
```

## Expected Serial Monitor output after sketch upload, BOOT, and reset

```text
ESP32-S3 is alive
tick
tick
tick
```

If this output appears, the ESP32-S3, USB cable, Arduino IDE, selected board, selected port, upload process, reset button, and Serial Monitor are working.
