# Tardi Web Controller

- Wi-Fi AP / captive portal starts automatically while the ESP32 is powered.
- No physical setup button is required for web access.
- Hotspot: `TARDI-LED`
- Manual page address: `http://192.168.4.1`
- Phone page controls LED look/feel settings only.
- Live sliders update LED appearance immediately.
- `SAVE` stores current look settings.
- `RESET` restores defaults in RAM until saved.
- The web page does not include FIRE, relay, or hardware validation controls.
- Existing captive portal routes remain available so phones may offer the page automatically after joining the hotspot.
