# IC6 4-Wire Interface

## Connections

The console harness carries four functions:

```text
Bike electronics
     |
     +---- GND
     +---- VIN
     +---- SDA
     +---- SCL
     |
Custom controller
```

Known harness observations:

- Black: GND
- Brown: VIN
- The remaining two conductors are the I²C signal pair.

The exact color assignment of those two signal conductors should not be assumed across every harness. Identify SDA/SCL on the bike you are using.

## Signal voltage

SDA and SCL have been measured idling near **5 V** on the tested IC6 hardware.

They are open-drain I²C lines. A high idle voltage is therefore normal because of the bus pull-up resistors.

### 5 V Arduino boards

A 5 V-compatible Arduino is the simplest starting point.

Use the board's normal SDA and SCL pins.

Examples:

| Board | SDA | SCL |
|---|---:|---:|
| Arduino Uno / Nano | A4 | A5 |
| Arduino Mega 2560 | 20 | 21 |
| Arduino Leonardo | 2 | 3 |

### 3.3 V controllers

Do **not** connect the IC6 signal lines directly to 3.3 V-only GPIO.

Use a proper bidirectional I²C level shifter for devices such as:

- ESP32
- Raspberry Pi
- Jetson
- many modern ARM SBCs
- 3.3 V microcontrollers

## Powering your controller

For initial testing, power the Arduino/ESP32/SBC from its normal power input and connect **GND, SDA, and SCL** to the bike.

Do not use the IC6 VIN wire to power a new controller until you have designed and checked the power stage for that controller.

This avoids accidental overloading or back-powering while you are only testing the data bus.

## Bike wake-up

The bike electronics may need pedaling before the I²C devices become active.

If an I²C scan finds nothing:

1. confirm common ground;
2. pedal the bike;
3. confirm the signal voltage;
4. power off before changing wiring;
5. verify or swap the two signal-line assignments if SDA/SCL are uncertain;
6. scan again for `0x40` and `0x48`.

## Basic replacement-controller wiring

```text
IC6 GND  -------- Controller GND
IC6 SDA  -------- SDA
IC6 SCL  -------- SCL

IC6 VIN  -------- leave disconnected for first tests
```

For a 3.3 V controller:

```text
IC6 SDA ----+
            |  bidirectional
            +-- I2C level shifter -- Controller SDA

IC6 SCL ----+
            |  bidirectional
            +-- I2C level shifter -- Controller SCL

IC6 GND --------------------------- Controller GND
```

## First electrical test

Before sending any commands:

1. power the controller correctly;
2. connect GND;
3. connect SDA/SCL;
4. pedal to wake the bike;
5. run an I²C scanner;
6. confirm:
   - `0x40`
   - `0x48`

Once both addresses appear, move to the Arduino reader example or the packet reference in `PROTOCOL.md`.
