# Tardi Web Controller

The ESP32 hosts the Tardi LED web controller while powered.

```text
SSID:     TARDI-LED
Password: tardigrade
Address:  http://192.168.4.1
```

The Wi-Fi AP and captive portal start automatically after boot. No physical setup button is required for web access.

## Purpose

The web page controls global LED look/feel settings for the sculpture.

It does not include:

- FIRE controls
- relay controls
- hardware test controls
- per-channel Output Expander test controls

## Controls

Current web controls include:

- target: whole sculpture, ambient, animation, or a selected zone
- brightness
- colour intensity / saturation
- speed
- palette
- behaviour
- animation duration

The animation-duration control applies to normal button-triggered LED animations and the all-zone Big Poof LED animation. It does not change FIRE pulse timing or the Big Poof FIRE cutoff.

## Save / Reset

Live changes apply immediately in RAM.

```text
SAVE  = persist current LED settings to flash
RESET = restore defaults in RAM until saved
```

Moving sliders or changing selectors does not constantly write flash.

## Captive Portal

The captive portal routes make phones more likely to offer the control page after joining `TARDI-LED`.

Manual access remains:

```text
http://192.168.4.1
```

## Safety

The web page must respect the existing firmware output gates and runtime paths.

It must not bypass:

- FIRE safety behaviour
- Output Expander guard/state
- PBDriverAdapter runtime logic
- button/FIRE/Big Poof timing
