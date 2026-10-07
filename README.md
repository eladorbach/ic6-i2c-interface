<img width="1152" height="1536" alt="Life Fitness ICG IC6 indoor bike console and electronics used for custom I2C controller integration" src="https://github.com/user-attachments/assets/643c8f90-a7e4-4bbb-b359-8706f9bb579a" />

<img width="1447" height="1087" alt="Life Fitness ICG IC6 I2C interface project with custom controller and dashboard" src="https://github.com/user-attachments/assets/a77466a1-6353-4e53-a517-3aadc3ca91b3" />

# Life Fitness / ICG IC6 I²C Interface

A practical hardware and protocol reference for connecting the **Life Fitness / ICG IC6 indoor cycling bike** to custom controllers such as **Arduino, ESP32, Raspberry Pi, Jetson, and other Linux SBCs**.

This project documents the IC6 four-wire console connection, I²C addresses and packets, DB9 wiring, cadence reads, raw brake-position data, battery/charger state, checksums, and example code for building custom displays, BLE bridges, ride loggers, and replacement consoles.

> **Independent project.** This is an independent interoperability project and is not affiliated with, authorized by, sponsored by, or endorsed by Life Fitness or Indoor Cycling Group (ICG). Product and company names are used only to identify compatible hardware; all trademarks belong to their respective owners.

## Scope and provenance

This repository is intended for **hardware interoperability, repair, experimentation, and custom-controller development**.

- The code and documentation in this repository were written for this independent project.
- Protocol fields, connector mappings, packet formats, checksums, measurements, and compatibility values are documented so independently built controllers can communicate with tested IC6 hardware.
- The repository does **not** distribute Life Fitness / ICG firmware, manufacturer source code, update packages, or proprietary UI artwork.
- Numerical compatibility data used by the Arduino dashboard is identified as such and is separated from manufacturer software.
- Observations are based on the tested IC6 hardware/software revision; verify electrical characteristics and behavior on your own hardware before connecting or writing to the bus.

See [NOTICE.md](NOTICE.md) for the project boundary, attribution, and trademark statement.

## Quick start

The IC6 console connection uses two 7-bit I²C addresses:

```text
Gear Module   0x40
Power Module  0x48
```

The normal request start byte is:

```text
0x11
```

Normal responses start with:

```text
0xAA  Gear Module
0x55  Power Module
```

### Read cadence

Send to `0x40`:

```text
11 03 04 18
```

Expected reply:

```text
AA 03 06 PERIOD_HI PERIOD_LO CHECKSUM
```

Decode:

```cpp
uint16_t period =
    ((uint16_t)reply[3] << 8) |
    reply[4];

float cadenceRPM =
    240000.0 / period / 9.5;
```

### Read raw brake position

Send to `0x40`:

```text
11 02 04 17
```

Expected reply:

```text
AA 02 06 RAW_HI RAW_LO CHECKSUM
```

Decode:

```cpp
uint16_t rawBrake =
    ((uint16_t)reply[3] << 8) |
    reply[4];
```

## What comes directly from I²C

The protocol provides data including:

- cadence sensor period
- raw brake position
- battery / charger state
- supply measurements
- brake calibration record
- gear-offset calibration record
- module state and service information

## What your controller calculates

These are normally calculated by your own controller rather than received as finished I²C values:

- displayed resistance
- watts
- speed
- distance
- calories
- averages
- training zones
- graphs
- workout history

That separation matters: the I²C layer provides the bike data, while your application decides how to turn it into the user interface and ride metrics.

## IC6 wiring and DB9 pinout

The harness has four functions:

| Function | Description |
|---|---|
| GND | Pin 8 |
| VIN | Pin 6 |
| SDA | Pin 9 |
| SCL | Pin 5 |

Read the **[IC6 wiring and DB9 pinout guide](docs/WIRING.md)** before connecting a controller.

The signal lines have been measured idling near **5 V** on the tested IC6 hardware. Use a bidirectional I²C level shifter with 3.3 V-only devices.

## IC6 I²C protocol reference

The complete **[Life Fitness / ICG IC6 I²C protocol reference](docs/PROTOCOL.md)** includes:

- packet format
- checksums
- normal telemetry reads
- calibration reads
- Power Module command table
- Gear Module command list
- safe polling guidance

## Arduino IC6 reader

For the easiest starting point, use the **[Arduino IC6 I²C reader example](examples/arduino-ic6-reader/arduino-ic6-reader.ino)**.

It:

- scans for `0x40` and `0x48`
- reads raw brake position
- reads cadence
- reads battery / charger state
- reads the three supply values
- validates response headers, lengths, and checksums

For a non-transmitting wiring/activity check, use the **[passive IC6 I²C bus indicator](examples/arduino-passive-bus-indicator/arduino-passive-bus-indicator.ino)**.

It watches SDA/SCL as high-impedance inputs and flashes the built-in LED when bus transitions are detected. It does not decode packets or transmit on the bus.

## Arduino ezLCD 320×240 dashboard

For the full color display build, see the **[Arduino + ezLCD 320×240 IC6 dashboard](examples/arduino-ezlcd-dashboard/README.md)**.

It uses an Arduino Uno and EarthLCD arLCD / ezLCD display to show live cadence, resistance, power, raw brake position, battery state, and the three IC6 supply voltages in an eight-card dashboard.[^dashboard-generator][^bus-speed]

[^dashboard-generator]: The dashboard's on-screen **MOTOR VOLTAGE** label is the first value returned by Power Module command `0x04`. During interoperability analysis of the tested IC6 console, this supply was identified as **UGEN / generator voltage**; the legacy display wording is retained only to match the original test-screen style.

[^bus-speed]: The ezLCD dashboard uses **50 kHz**, matching the bus rate observed and tested on the reference IC6 console for that build. The generic Arduino reader starts at **100 kHz** as a conventional conservative Arduino I²C setting. The difference is intentional; use 50 kHz when reproducing the tested ezLCD setup.

## Recommended first build

1. Power your controller normally.
2. Connect grounds.
3. Connect SDA and SCL with the correct voltage interface.
4. Pedal the bike to wake its electronics.
5. Scan for `0x40` and `0x48`.
6. Read cadence.
7. Read raw brake position.
8. Add Power Module state reads.
9. Build your display, logger, BLE bridge, or touchscreen UI around those values.

Start with reads only. Calibration and control writes are documented, but they are not required for normal ride telemetry.

## Project goal

The goal is simple: make the Life Fitness / ICG IC6 easy to integrate with custom I²C hardware.

A small Arduino can be used as a basic reader, while a larger ESP32, Raspberry Pi, Jetson, or other SBC can use the same packets for a full touchscreen console.

More hardware projects and build documentation are available at my blog elad orbach **[eladorbach.com](https://eladorbach.com)**.

## License

MIT License. See [LICENSE](LICENSE).
