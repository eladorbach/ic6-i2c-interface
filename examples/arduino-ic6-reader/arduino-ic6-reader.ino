/*
  Life Fitness / ICG IC6 I2C reader

  Read-only starter for a 5 V-compatible Arduino.

  Reads:
    - Gear Module 0x40 raw brake
    - Gear Module 0x40 cadence
    - Power Module 0x48 charger/battery state
    - Power Module 0x48 supply values

  Wiring:
    IC6 GND -> Arduino GND
    IC6 SDA -> Arduino SDA
    IC6 SCL -> Arduino SCL

  For a 3.3 V controller, use a bidirectional I2C level shifter.

  Power the controller from its normal supply for initial testing.
  Leave IC6 VIN disconnected until your controller power design is verified.
*/

#include <Wire.h>

static const uint8_t GEAR_ADDR  = 0x40;
static const uint8_t POWER_ADDR = 0x48;

uint8_t gearChecksum(const uint8_t *data, size_t length)
{
  uint8_t sum = 0;

  for (size_t i = 0; i < length; ++i)
    sum += data[i];

  return sum;
}

uint8_t powerChecksum(const uint8_t *data, size_t length)
{
  uint8_t sum = 0;

  for (size_t i = 0; i < length; ++i)
    sum += data[i] * (1 + (i & 1));

  return sum;
}

bool writeThenRead(
    uint8_t address,
    const uint8_t *request,
    uint8_t requestLen,
    uint8_t *reply,
    uint8_t replyLen)
{
  Wire.beginTransmission(address);
  Wire.write(request, requestLen);

  if (Wire.endTransmission() != 0)
    return false;

  delay(2);

  uint8_t received =
      Wire.requestFrom((int)address, (int)replyLen);

  if (received != replyLen)
  {
    while (Wire.available())
      Wire.read();

    return false;
  }

  for (uint8_t i = 0; i < replyLen; ++i)
  {
    if (!Wire.available())
      return false;

    reply[i] = Wire.read();
  }

  return true;
}

bool validGearReply(
    const uint8_t *reply,
    uint8_t replyLen,
    uint8_t command)
{
  if (replyLen < 4)
    return false;

  if (reply[0] != 0xAA)
    return false;

  if (reply[1] != command)
    return false;

  if (reply[2] != replyLen)
    return false;

  return
      gearChecksum(reply, replyLen - 1) ==
      reply[replyLen - 1];
}

bool validPowerReply(
    const uint8_t *reply,
    uint8_t replyLen,
    uint8_t command)
{
  if (replyLen < 4)
    return false;

  if (reply[0] != 0x55)
    return false;

  if (reply[1] != command)
    return false;

  if (reply[2] != replyLen)
    return false;

  return
      powerChecksum(reply, replyLen - 1) ==
      reply[replyLen - 1];
}

bool readGear(
    uint8_t command,
    uint8_t *reply,
    uint8_t replyLen)
{
  uint8_t request[4] = {
    0x11,
    command,
    0x04,
    0x00
  };

  request[3] =
      gearChecksum(request, 3);

  if (!writeThenRead(
          GEAR_ADDR,
          request,
          sizeof(request),
          reply,
          replyLen))
    return false;

  return
      validGearReply(
          reply,
          replyLen,
          command);
}

bool readPower(
    uint8_t command,
    uint8_t *reply,
    uint8_t replyLen)
{
  /*
    This helper is for the normal 4-byte checksummed Power Module
    read commands such as 0x01, 0x02, 0x04, 0x06, 0x0B,
    0x0D, 0x31 and 0x33.
  */

  uint8_t request[4] = {
    0x11,
    command,
    0x04,
    0x00
  };

  request[3] =
      powerChecksum(request, 3);

  if (!writeThenRead(
          POWER_ADDR,
          request,
          sizeof(request),
          reply,
          replyLen))
    return false;

  return
      validPowerReply(
          reply,
          replyLen,
          command);
}

void scanBus()
{
  Serial.println("I2C scan:");

  for (uint8_t address = 1;
       address < 127;
       ++address)
  {
    Wire.beginTransmission(address);
    uint8_t error =
        Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("  found 0x");

      if (address < 16)
        Serial.print('0');

      Serial.println(address, HEX);
    }
  }

  Serial.println();
}

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 3000)
  {
  }

  Wire.begin();

  // Conservative starting speed.
  Wire.setClock(100000);

  Serial.println();
  Serial.println("IC6 I2C reader");
  Serial.println("Pedal the bike if 0x40 / 0x48 are not awake.");
  Serial.println();

  scanBus();
}

void loop()
{
  uint8_t brakeReply[6];
  uint8_t cadenceReply[6];

  bool brakeOK =
      readGear(
          0x02,
          brakeReply,
          sizeof(brakeReply));

  bool cadenceOK =
      readGear(
          0x03,
          cadenceReply,
          sizeof(cadenceReply));

  if (brakeOK)
  {
    uint16_t rawBrake =
        ((uint16_t)brakeReply[3] << 8) |
        brakeReply[4];

    Serial.print("raw_brake=");
    Serial.print(rawBrake);
  }
  else
  {
    Serial.print("raw_brake=NA");
  }

  Serial.print("  ");

  if (cadenceOK)
  {
    uint16_t rawPeriod =
        ((uint16_t)cadenceReply[3] << 8) |
        cadenceReply[4];

    if (rawPeriod != 0)
    {
      float sensorRPM =
          240000.0 / rawPeriod;

      float cadenceRPM =
          sensorRPM / 9.5;

      Serial.print("cadence=");
      Serial.print(cadenceRPM, 1);
      Serial.print(" rpm");
    }
    else
    {
      Serial.print("cadence=0");
    }
  }
  else
  {
    Serial.print("cadence=NA");
  }

  static uint32_t lastPowerRead = 0;

  if (millis() - lastPowerRead >= 1000)
  {
    lastPowerRead = millis();

    uint8_t stateReply[6];

    if (readPower(
            0x06,
            stateReply,
            sizeof(stateReply)))
    {
      uint8_t chargerState =
          stateReply[3];

      uint8_t batteryState =
          stateReply[4];

      Serial.print("  charger=");
      Serial.print(chargerState);
      Serial.print("  battery=");
      Serial.print(batteryState);
    }
    else
    {
      Serial.print("  charger=NA  battery=NA");
    }

    uint8_t supplyReply[10];

    if (readPower(
            0x04,
            supplyReply,
            sizeof(supplyReply)))
    {
      uint16_t value1 =
          ((uint16_t)supplyReply[3] << 8) |
          supplyReply[4];

      uint16_t value2 =
          ((uint16_t)supplyReply[5] << 8) |
          supplyReply[6];

      uint16_t value3 =
          ((uint16_t)supplyReply[7] << 8) |
          supplyReply[8];

      Serial.print("  supply=");
      Serial.print(value1);
      Serial.print(',');
      Serial.print(value2);
      Serial.print(',');
      Serial.print(value3);
    }
    else
    {
      Serial.print("  supply=NA");
    }
  }

  Serial.println();

  delay(250);
}
