# Flashing — playa card

> This is the **combo lighting** build. `../EclairTardiSplit/` is the baseline
> fallback; `Tardi_Fire` and `LCD_Driver_Test` are identical in both.

Same settings for **every** sketch in this folder. The IDE does not read
`sketch.yaml`, so set these by hand in **Tools** each time.

```
Board            : ESP32S3 Dev Module
USB CDC On Boot  : Enabled          <-- the one everyone forgets
PSRAM            : OPI PSRAM
Flash Size       : 8MB (64Mb)
Upload Speed     : 115200
Port             : Tools > Port > /dev/cu.usbmodem...

Serial Monitor   : 115200
```

## Plug into `USB`, not `UART`

Two USB-C sockets on the DevKitC. `UART` goes through the bridge chip on
GPIO43/44. With CDC enabled, serial only comes out of the native `USB` one.

> **A blank Serial Monitor is almost always one of these two things** — CDC
> still Disabled, or the cable in the UART socket. Both look exactly like a
> crashed sketch. Check them before you debug anything.

---

## Which sketch

| | Board | Libraries |
|---|---|---|
| `Tardi_Fire/` | fire board | Adafruit SSD1306, Adafruit GFX |
| `Eclair_LED/` | LED board | FastLED 3.9+ |
| `LCD_Driver_Test/` | either — bring-up tool | FastLED 3.9+ |

**On new hardware, flash `LCD_Driver_Test` first.** Safe with no LEDs attached.

---

## Tardi — the fire board

```
TARDI - FIRE CONTROLLER
interlock    : ON
```

Will **not arm** until every button has been seen low. OLED blinks `NOT ARMED`
— release everything, then:

```
*** ARMED - all inputs seen low, fire outputs live ***
```

Commands: `poof` `status` `counts` `arm`

> **`FIRE_OUTPUTS_ENABLED = true`** — this drives the real relays. For dry
> bench testing set it `false` near the top of the `.ino`.

Behaviour:

```
button N     ->  FIRE N, 100 ms, repeating every 1 s while held
all 8 held   ->  all zones fire TOGETHER 1.0 s, FIRE9 1.5 s
                 then 30 s lockout on EVERY poofer
```

---

## Eclair — the LED board

**Check `build_opt.h` appears as a second tab.** Without it FastLED never gets
the LCD flags, falls back to RMT, and you are capped at 4 lanes — half the
sculpture stays dark.

Success looks like:

```
driver : 8/8 on LCD_CLOCKLESS  ->  PASS, all lanes parallel
```

Anything less than 8/8 means it fell back. Check `build_opt.h`.

Commands:

```
status  driver  heap  buttons  colours
led on|off|solid    led ch 0..7    led red|green|blue
trigger 1..8        trigger all    mood
```

`buttons` samples all 8 inputs for a second. **With nothing pressed every pin
should read 0%.** Anything above that is noise on the wiring, and it will show
up as zones triggering on their own.

---

## If something looks wrong

| Symptom | Look at first |
|---|---|
| Blank Serial Monitor | CDC setting, then which USB socket |
| `driver : 4/8` or `RMT` | `build_opt.h` missing from the sketch folder |
| Zones triggering on their own | run `buttons` — floating inputs |
| Colours look wrong | `led red` / `led green` / `led blue` to check byte order |
| One lane dead | `led ch N` to isolate it |
| Fire won't trigger | `poof` — probably still in the 30 s lockout |
| Nothing fires at all | OLED says `NOT ARMED`? Release all buttons |

---

## Not fixed in software

- **The SN74AHCT244 buffer** — until it exists, strips run at 3.3 V against a
  3.5 V threshold. Inconsistent behaviour between lanes is expected, not a bug.
- **Button RC filtering** — 1k series + 100nF at each input. The 150 ms
  software filter is mitigation, not a cure.
- **LED power off a bus bar**, never through a controller board. That is what
  killed the expander.
