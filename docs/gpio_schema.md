# GPIO Schema

Exact pin ownership for the two-board Eclair7 production build.

## Tardi

| Function | GPIO |
|---|---:|
| Button 1-4 | 4, 5, 6, 7 |
| Button 5-8 | 15, 16, 17, 18 |
| FIRE1-FIRE7 | 8, 9, 10, 11, 12, 13, 14 |
| FIRE8 | 21 |
| FIRE9 / Head Poof | 47 |
| UART1 TX to Eclair RX | 40 |
| UART1 RX from Eclair TX | 41 |
| Native USB D-/D+ | 19, 20 |

Buttons are active-HIGH with external 10k pull-downs. FIRE is active-LOW:
HIGH is idle and LOW is triggered. Tardi drives no LED DIN.

## Eclair

| Function | GPIO |
|---|---:|
| Z1 (208) | 4 |
| Z2 (325) | 5 |
| Z3 (400) | 6 |
| Z4 (300) | 7 |
| Z5 (300) | 8 |
| Z6 (300) | 9 |
| Z7 (75) | 10 |
| UART1 RX from Tardi TX | 18 |
| UART1 TX to Tardi RX | 17 |
| Native USB D-/D+ | 19, 20 |

The seven LED lanes use GRB order and contain 1,908 pixels. The link is
2,000,000 baud, 8N1, full duplex, with common board and LED-power ground.

OLED/I2C, SPI, Pixelblaze Output Expander, Z8, and a physical web-setup button
have no active GPIO ownership on either production firmware.
