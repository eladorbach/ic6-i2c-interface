<img width="4032" height="3024" alt="IMG_1422" src="https://github.com/user-attachments/assets/734dc46a-2417-406d-a7e4-319b55058349" />
# Arduino + ezLCD 320×240 IC6 dashboard

A live 320×240 dashboard for the Life Fitness / ICG IC6 using an **Arduino Uno** and an **EarthLCD arLCD / ezLCD 3xx** display.

## Hardware and wiring

| IC6 DB9 | Function | Arduino Uno / arLCD |
|---|---|---|
| Pin 9 | SDA | A4 / SDA |
| Pin 5 | SCL | A5 / SCL |
| Pin 8 | GND | GND |
| Pin 6 | VIN | **Leave disconnected** |

The sketch runs the IC6 I²C bus at **50 kHz**.[^bus-speed]

## Display layout

Top: **blue → darker yellow → red → darker green**

Bottom: **darker green → blue → darker yellow → red**

The eight cards show cadence, resistance, power, raw brake position, battery state, generator voltage,[^generator-label] battery voltage, and system voltage. The cards use 3 px frames with lightly rounded corners.

## Required library

Install EarthLCD **arLCDLib / ezLCDLib** separately (GitHub: `earthlcd/arLCDLib`). It is not vendored here because it carries its own CC BY-SA license.

## Files

- `arduino-ezlcd-dashboard.ino` — live IC6 reader and dashboard
- `Ic6PowerCurveReference.h` — numerical IC6 compatibility curve used by this example's power calculation; see the provenance note in the file header

## Interoperability data

The power-curve header contains numerical compatibility coefficients used to reproduce the observed IC6 power-calculation behavior on the tested bike. It is provided for interoperability with independently written controller code. It does **not** contain manufacturer firmware, manufacturer source code, update packages, or proprietary UI assets.

## Polling

- Gear Module `0x40`: 4 Hz
- Power Module `0x48`: 1 Hz
- static calibration refresh: every 30 s

The sketch validates the normal IC6 response headers, lengths, and checksums before using data. It deliberately does not issue the OEM boot-time SMBus-style `0x48 / 0x11` read before normal traffic.

See [../../docs/PROTOCOL.md](../../docs/PROTOCOL.md) for the protocol details.

[^generator-label]: The corresponding on-screen card says **MOTOR VOLTAGE** to preserve the original IC6 test-display wording. It is the first 16-bit value returned by Power Module command `0x04`, identified during interoperability analysis of the tested IC6 console as **UGEN / generator voltage**.

[^bus-speed]: **50 kHz** is the rate observed and tested on the reference IC6 console used by this dashboard. The simpler `arduino-ic6-reader` example starts at **100 kHz** as a conventional Arduino I²C setting; that difference is intentional rather than a protocol contradiction.
