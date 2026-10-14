# Hardware

## Bill of materials

| Qty | Part | Role |
|---|---|---|
| 1 | ESP32-S3 dev board (YD-ESP32-S3 / ESP32-S3-N16R8) | MCU, Wi-Fi access point, web server |
| 8 | IRLB8721 logic-level N-MOSFET (IRLZ44N also works) | low-side switch, one per lighting channel |
| 8 | 220 Ω gate resistor + 10 kΩ gate pull-down (recommended) | limits gate current / keeps the channel off during boot |
| 1 | LM2596 buck converter → 5 V | powers the ESP32 from the 12 V rail |
| 1 | Mean Well LRS-100-12 (12 V, 8.5 A) | main supply for the LED strips |
| — | KRE screw terminals, perfboard, 12 V LED strips | wiring per zone |

## Pin map

| Channel | GPIO | Default name (v1) |
|---|---|---|
| 1 | 1  | Fachada (façade) |
| 2 | 2  | Interior |
| 3 | 42 | Jardim (garden) |
| 4 | 41 | Piscina (pool) |
| 5 | 40 | Garagem (garage) |
| 6 | 39 | Telhado (roof) |
| 7 | 38 | Acento 1 (accent) |
| 8 | 37 | Acento 2 (accent) |

PWM: 5 kHz, 10-bit (0–1023) via LEDC. Names are editable from the UI and persisted in NVS.

## Wiring (per channel)

```
 12 V ──────────────┬──────────── LED strip (+)
                    │
              LED strip (−)
                    │
                  Drain
 GPIO ──220 Ω── Gate      IRLB8721
           │      Source ─────────── GND (common with ESP32)
         10 kΩ
           │
          GND
```

- The 12 V supply and the ESP32 **must share ground**.
- IRLB8721 is fully on at 3.3 V gate drive (logic-level), so no gate driver is needed at these currents.
- Size the wiring per zone for its strip current; keep each channel well under the PSU rating in total.
