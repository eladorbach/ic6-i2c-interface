# IC6 I²C Interface

A practical I²C reference for building custom controllers for the **Life Fitness / ICG IC6** indoor bike.

Use the bike's existing electronics with your own:

- Arduino
- ESP32
- Raspberry Pi
- Linux SBC
- touchscreen display
- BLE bridge
- Wi-Fi controller
- ride logger

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

## Wiring

The harness has four functions:

| Function | Description |
|---|---|
| GND | Ground |
| VIN | Bike-side power rail |
| SDA | I²C data |
| SCL | I²C clock |

See [docs/WIRING.md](docs/WIRING.md) before connecting a controller.

The signal lines have been measured idling near **5 V** on the tested IC6 hardware. Use a bidirectional I²C level shifter with 3.3 V-only devices.

## Protocol

The complete integration reference is in:

- [docs/PROTOCOL.md](docs/PROTOCOL.md)

It includes:

- packet format
- checksums
- normal telemetry reads
- calibration reads
- Power Module command table
- Gear Module command list
- safe polling guidance

## Arduino

For the easiest starting point, use:

```text
examples/arduino-ic6-reader/arduino-ic6-reader.ino
```

It:

- scans for `0x40` and `0x48`
- reads raw brake position
- reads cadence
- reads battery / charger state
- reads the three supply values
- validates response headers, lengths, and checksums

A separate passive activity indicator can also be kept for basic wiring tests.

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

The goal is simple: make the IC6 easy to integrate with custom I²C hardware.

A small Arduino can be used as a basic reader, while a larger ESP32, Raspberry Pi, Jetson, or other SBC can use the same packets for a full touchscreen console.

## License

MIT License. See [LICENSE](LICENSE).
