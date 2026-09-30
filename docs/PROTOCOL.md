# IC6 I²C Protocol

Practical application-level protocol reference for custom controllers, displays, loggers, and bridges for the **Life Fitness / ICG IC6**.

All addresses below are **7-bit I²C addresses**.

## Bus summary

| Device | Address | Normal response start |
|---|---:|---:|
| Gear Module | `0x40` | `0xAA` |
| Power Module | `0x48` | `0x55` |

Normal request frames start with:

```text
0x11
```

For the normal packet format:

```text
byte 0 = start byte
byte 1 = command
byte 2 = total frame length
```

Multi-byte scalar values are big-endian.

---

# 1. Quick read-only command set

These commands are enough for most custom displays.

| Device | Function | Request | Reply bytes |
|---|---|---|---:|
| `0x40` | Generic 16-bit module value | `11 01 04 16` | 6 |
| `0x40` | Raw brake position | `11 02 04 17` | 6 |
| `0x40` | Cadence period | `11 03 04 18` | 6 |
| `0x48` | Supply measurements | `11 04 04 1D` | 10 |
| `0x48` | Charger + battery state | `11 06 04 21` | 6 |
| `0x48` | Read brake calibration | `11 31 04 77` | 11 |
| `0x48` | Read gear-offset calibration | `11 33 04 7B` | 10 |

---

# 2. Gear Module — `0x40`

## Checksummed telemetry frame

The normal telemetry requests use an ordinary 8-bit byte-sum checksum:

```cpp
uint8_t gearChecksum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; ++i)
        sum += data[i];

    return sum;
}
```

Example:

```text
11 + 02 + 04 = 17
```

so the raw-brake request is:

```text
11 02 04 17
```

For a checksummed Gear response, validate the checksum over every byte except the final checksum byte.

## Command `0x01` — generic 16-bit value

Request:

```text
11 01 04 16
```

Reply:

```text
AA 01 06 VALUE_HI VALUE_LO CHECKSUM
```

Decode:

```cpp
uint16_t value =
    ((uint16_t)reply[3] << 8) |
    reply[4];
```

## Command `0x02` — raw brake position

Request:

```text
11 02 04 17
```

Reply:

```text
AA 02 06 RAW_HI RAW_LO CHECKSUM
```

Decode:

```cpp
uint16_t rawBrake =
    ((uint16_t)reply[3] << 8) |
    reply[4];
```

The displayed 0–100 resistance value is calculated from the raw value and brake calibration data.

## Command `0x03` — cadence sensor period

Request:

```text
11 03 04 18
```

Reply:

```text
AA 03 06 PERIOD_HI PERIOD_LO CHECKSUM
```

Decode:

```cpp
uint16_t rawPeriod =
    ((uint16_t)reply[3] << 8) |
    reply[4];

float sensorRPM =
    240000.0 / rawPeriod;

float cadenceRPM =
    sensorRPM / 9.5;
```

Check `rawPeriod != 0` before dividing.

## Gear Module command list

| Command | Function |
|---:|---|
| `0x01` | Generic 16-bit module value |
| `0x02` | Raw brake position |
| `0x03` | Cadence sensor period |
| `0x07` | Configuration write |
| `0x0C` | Configuration write with two 16-bit values |
| `0x0D` | Configuration write |
| `0x0E` | Configuration / service query |
| `0x51` | Update / service control |
| `0x52` | Variable-length data transfer |
| `0x53` | Update / service transfer with two 16-bit values |
| `0x54` | Update / service control |
| `0x55` | Status / ACK; known success value `0x88` |
| `0x56` | Status / ACK; known success value `0x01` |
| `0x61` | Module identify / protocol probe |

The normal replacement-controller data path only needs `0x02` and `0x03`, plus calibration data from the Power Module.

---

# 3. Power Module — `0x48`

## Weighted checksum

When a Power Module command uses a checksum, the weights alternate:

```text
1, 2, 1, 2, 1, 2, ...
```

starting at byte 0.

```cpp
uint8_t powerChecksum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; ++i)
        sum += data[i] * (1 + (i & 1));

    return sum;
}
```

Example for command `0x06`:

```text
11×1 + 06×2 + 04×1 = 21 hex
```

Request:

```text
11 06 04 21
```

Some Power Module commands intentionally do **not** use checksums. The command table below shows which frames do.

## Command `0x04` — supply measurements

Request:

```text
11 04 04 1D
```

Reply length:

```text
10 bytes
```

Reply structure:

```text
55 04 0A V1_HI V1_LO V2_HI V2_LO V3_HI V3_LO CHECKSUM
```

Decode:

```cpp
uint16_t value1 =
    ((uint16_t)reply[3] << 8) |
    reply[4];

uint16_t value2 =
    ((uint16_t)reply[5] << 8) |
    reply[6];

uint16_t value3 =
    ((uint16_t)reply[7] << 8) |
    reply[8];
```

## Command `0x06` — charger and battery state

Request:

```text
11 06 04 21
```

Reply:

```text
55 06 06 CHARGER BATTERY CHECKSUM
```

Decode:

```cpp
uint8_t chargerState = reply[3];
uint8_t batteryState = reply[4];
```

Charger-state values used by the normal IC6 power calculation correspond to these watt corrections:

```text
0 -> +0.0 W
1 -> +0.5 W
2 -> +2.5 W
3 -> +5.0 W
```

## Command `0x31` — read brake calibration

Request:

```text
11 31 04 77
```

Reply length:

```text
11 bytes
```

The reply contains the stored brake-calibration record and its status information.

## Command `0x33` — read gear-offset calibration

Request:

```text
11 33 04 7B
```

Reply length:

```text
10 bytes
```

The reply contains the stored gear-offset calibration record.

## Calibration writes

The paired write commands are:

```text
0x30 = write brake calibration
0x32 = write gear-offset calibration
```

They are not required for normal ride telemetry. Start with read-only communication and only use calibration writes intentionally.

---

# 4. Power Module command table

`TX` and `RX` are total frame lengths in **bytes**.

`TX checksum` and `RX checksum` tell you whether that direction includes the weighted checksum.

| CMD | TX | TX checksum | RX | RX checksum | Function |
|---:|---:|:---:|---:|:---:|---|
| `0x01` | 4 | Yes | 5 | Yes | One-byte state / diagnostic |
| `0x02` | 4 | Yes | 6 | Yes | Two-byte module information / measurement |
| `0x04` | 4 | Yes | 10 | Yes | Three 16-bit supply measurements |
| `0x06` | 4 | Yes | 6 | Yes | Charger + battery state |
| `0x0B` | 4 | Yes | 28 | Yes | Large diagnostic / configuration block |
| `0x0C` | 6 | Yes | 0 | No | Configuration write |
| `0x0D` | 4 | Yes | 6 | Yes | Two-byte configuration / diagnostic read |
| `0x11` | 4 | No | 0 | No | Control write |
| `0x12` | 4 | No | 0 | No | Control write |
| `0x13` | 3 | No | 5 | No | Short status query |
| `0x16` | 10 | Yes | 5 | Yes | Configuration / table update |
| `0x30` | 11 | Yes | 0 | No | Write brake calibration |
| `0x31` | 4 | Yes | 11 | Yes | Read brake calibration |
| `0x32` | 10 | Yes | 0 | No | Write gear-offset calibration |
| `0x33` | 4 | Yes | 10 | Yes | Read gear-offset calibration |
| `0x41` | 5 | Yes | 6 | Yes | Service / configuration query |
| `0x44` | 4 | Yes | 5 | Yes | Action / control with status reply |
| `0x51` | 4 | No | 0 | No | Update / service control |
| `0x52` | 68 (`0x44`) | No | 0 | No | Data-block transfer |
| `0x53` | 8 | No | 0 | No | Update address / length / control |
| `0x54` | 4 | No | 0 | No | Update / service control |
| `0x55` | 4 | No | 5 | No | Update / service status |
| `0x56` | 4 | No | 4 | No | Update / service status |
| `0x61` | 4 | No | 5 | No | Identify / version |
| `0x62` | 4 | No | 0 | No | Update / control flag |
| `0x63` | 4 | No | 4 | No | Short update / status reply |
| `0x71` | 3 | No | 0 | No | Service control |
| `0x72` | 8 | No | 0 | No | Service payload |
| `0x73` | 4 | No | 0 | No | Service control |

For custom ride displays, the advanced service/update rows can normally be ignored.

---

# 5. Response validation

## Gear Module normal telemetry

For a six-byte response such as:

```text
AA 02 06 HI LO CHECKSUM
```

check:

1. `reply[0] == 0xAA`
2. `reply[1] == expectedCommand`
3. `reply[2] == expectedLength`
4. ordinary byte-sum checksum matches

## Power Module checksummed commands

For commands whose table says `RX checksum = Yes`, check:

1. `reply[0] == 0x55`
2. `reply[1] == expectedCommand`
3. `reply[2] == expectedLength`
4. weighted checksum matches

Do not require a checksum for commands whose table says `RX checksum = No`.

---

# 6. Derived ride metrics

The I²C bus does not provide every finished display metric directly.

A custom controller normally combines:

```text
cadence period
raw brake position
brake calibration
gear offset
charger state
```

to create higher-level ride values.

For example:

```text
raw period -> sensor RPM -> cadence
raw brake + calibration -> displayed resistance
cadence + calibrated brake data -> power model
power + rider/application model -> speed / distance / calories
```

This keeps the protocol layer independent from the touchscreen, BLE, Wi-Fi, logging, or training application built on top of it.

---

# 7. Suggested polling

A simple starting schedule is:

```text
4 Hz    Gear 0x02  raw brake
4 Hz    Gear 0x03  cadence
1 Hz    Power 0x06 battery / charger
1 Hz    Power 0x04 supply values
startup / occasional:
        Power 0x31 brake calibration
        Power 0x33 gear-offset calibration
```

The bus protocol is independent of the UI refresh rate. A touchscreen can render smoothly while the numeric telemetry is updated at a lower rate.

---

# 8. Arduino

A complete read-only starter is included at:

```text
examples/arduino-ic6-reader/arduino-ic6-reader.ino
```

It implements the checksums, packet validation, bus scan, and the most useful telemetry reads.
