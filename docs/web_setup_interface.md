# Tardi Web Controller

```text
SSID: TARDI-LED
Password: tardigrade
Address: http://192.168.4.1
```

The AP and captive portal start automatically. No physical setup button is
required.

The controller serves one deterministic gzip-compressed static page. The page
loads live connection state and all current/saved LED values from the existing
`/api/status` endpoint when it opens and every five seconds afterward.

## Scope

The page edits LED appearance only:

- whole sculpture or selected zone;
- ambient or active animation look;
- brightness and colour intensity;
- speed, palette, and behavior;
- animation duration.

It has no FIRE, relay, or physical hardware-test controls.

## Save and Reset

- Live edits apply in RAM.
- `SAVE` persists the current LED settings to flash.
- `RESET` loads defaults into RAM until saved.
- Power cycling reloads the last saved compatible settings.

## Output Status

The connection card reports:

- `ON` during normal direct output;
- `ON / SETTINGS DARK` when current ambient brightness multipliers produce an entirely dark sculpture;
- whether the first `FastLED.show()` call has been attempted;
- current mode and direct lane GPIOs.

The five-second moving LED hardware check finishes before the AP/web server
starts, so it is reported over USB Serial rather than through the web page.

The internal `/api/mode` endpoint accepts `off`, `animation`, and `solid`,
but the normal page does not expose hardware-test buttons.

Web code must never alter FIRE pins, polarity, timing, or Head Poof behavior.
