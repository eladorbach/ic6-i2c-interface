/*
  EarthMake / EarthLCD arLCD - Life Fitness IC6 live I2C display

  Verified live IC6 female DE-9 mapping:
    P9 -> SDA -> arLCD A4
    P5 -> SCL -> arLCD A5
    P8 -> GND -> arLCD GND
    P6 -> VIN -> LEAVE DISCONNECTED

  Bus: 50 kHz
  Gear Module:  0x40
  Power Module: 0x48

  This build deliberately does NOT send the OEM boot-time 0x48/0x11
  SMBus read before normal protocol traffic.
*/

#include <Arduino.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <ezLCDLib.h>
#include "Ic6PowerCurveReference.h"

ezLCD3 lcd;

static const uint8_t GEAR_ADDR  = 0x40;
static const uint8_t POWER_ADDR = 0x48;

static uint32_t lastGearPoll = 0;
static uint32_t lastPowerPoll = 0;
static uint32_t lastStaticPoll = 0;
static bool staticLoaded = false;

static bool gearSeen = false;
static bool powerStateValid = false;
static bool supplyValid = false;
static bool calibrationValid = false;
static bool offsetValid = false;

static uint16_t rawBrake = 0;
static uint16_t rawPeriod = 0;
static uint16_t cadence10 = 0;

static uint16_t calStart = 0;
static uint16_t calEnd = 0;
static int8_t gearOffset = 0;

static uint8_t chargerState = 0;
static uint8_t batteryState = 0;
static uint16_t supply1 = 0, supply2 = 0, supply3 = 0;

static int16_t resistance = -1;
static int16_t powerWatts = -1;

static float powerWindow[10];
static uint8_t powerWindowCount = 0;
static uint8_t powerWindowPos = 0;
static float powerWindowSum = 0.0f;

// Custom color slots documented by EarthLCD examples.
static const uint8_t C_PANEL  = 168;
static const uint8_t C_WHITE  = 169;
static const uint8_t C_BLUE   = 170;
static const uint8_t C_RED    = 171;
static const uint8_t C_YELLOW = 172;
static const uint8_t C_GREEN  = 173;
static const uint8_t C_UI_BLUE   = 180;
static const uint8_t C_UI_YELLOW = 181;
static const uint8_t C_UI_RED    = 182;
static const uint8_t C_UI_GREEN  = 183;

static void putText(int x, int y, int color, const char *txt)
{
  lcd.color(color);
  lcd.xy(x, y);
  lcd.print(txt);
}

static void putCenteredSmall(int x, int w, int y, int color,
                             const char *txt, bool heavier)
{
  // Built-in font 1 is the compact face on this ezLCD. Its practical
  // character advance is ~6 px, which fits the full IC6 labels in 74 px.
  const int width = (int)strlen(txt) * 6;
  int tx = x + (w - width) / 2;
  if (tx < x + 2) tx = x + 2;

  lcd.font(1);
  putText(tx, y, color, txt);
  if (heavier)
    putText(tx + 1, y, color, txt);  // subtle 1 px overdraw for weight
}

static uint8_t gearChecksum(const uint8_t *data, uint8_t length)
{
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; ++i) sum += data[i];
  return sum;
}

static uint8_t powerChecksum(const uint8_t *data, uint8_t length)
{
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; ++i)
    sum += data[i] * (1 + (i & 1));
  return sum;
}

static bool writeThenRead(uint8_t address,
                          const uint8_t *request,
                          uint8_t requestLen,
                          uint8_t *reply,
                          uint8_t replyLen)
{
  Wire.beginTransmission(address);
  Wire.write(request, requestLen);
  if (Wire.endTransmission() != 0) return false;

  delay(2);

  const uint8_t received = Wire.requestFrom((int)address, (int)replyLen);
  if (received != replyLen) {
    while (Wire.available()) Wire.read();
    return false;
  }

  for (uint8_t i = 0; i < replyLen; ++i) {
    if (!Wire.available()) return false;
    reply[i] = Wire.read();
  }
  return true;
}

static bool readGear(uint8_t command, uint8_t *reply, uint8_t replyLen)
{
  uint8_t request[4] = {0x11, command, 0x04, 0x00};
  request[3] = gearChecksum(request, 3);

  if (!writeThenRead(GEAR_ADDR, request, 4, reply, replyLen))
    return false;

  if (reply[0] != 0xAA || reply[1] != command || reply[2] != replyLen)
    return false;

  return gearChecksum(reply, replyLen - 1) == reply[replyLen - 1];
}

static bool readPower(uint8_t command, uint8_t *reply, uint8_t replyLen)
{
  uint8_t request[4] = {0x11, command, 0x04, 0x00};
  request[3] = powerChecksum(request, 3);

  if (!writeThenRead(POWER_ADDR, request, 4, reply, replyLen))
    return false;

  if (reply[0] != 0x55 || reply[1] != command || reply[2] != replyLen)
    return false;

  return powerChecksum(reply, replyLen - 1) == reply[replyLen - 1];
}

static void resetPowerAverage()
{
  powerWindowCount = 0;
  powerWindowPos = 0;
  powerWindowSum = 0.0f;
}

static float averagePower(float value)
{
  if (powerWindowCount < 10) {
    powerWindow[powerWindowPos] = value;
    powerWindowSum += value;
    ++powerWindowCount;
  } else {
    powerWindowSum -= powerWindow[powerWindowPos];
    powerWindow[powerWindowPos] = value;
    powerWindowSum += value;
  }

  powerWindowPos = (powerWindowPos + 1) % 10;
  return powerWindowSum / powerWindowCount;
}

static void updateProcessedRideValues()
{
  resistance = -1;
  powerWatts = -1;

  if (!gearSeen || !calibrationValid || calStart == calEnd)
    return;

  int value = (int)rawBrake;

  if (calStart < calEnd) {
    if (value < (int)calStart) value = (int)calStart;
  } else {
    if (value > (int)calStart) value = (int)calStart;
  }

  const int span = abs((int)calEnd - (int)calStart);
  if (span <= 0) return;

  int delta = (calStart < calEnd)
      ? value - (int)calStart
      : (int)calStart - value;

  if (delta < 0) delta = 0;

  long r = (100L * delta + span / 2) / span;
  if (r < 0) r = 0;
  if (r > 100) r = 100;
  resistance = (int16_t)r;

  if (!powerStateValid || rawPeriod == 0) {
    resetPowerAverage();
    return;
  }

  const float cadence = (240000.0f / (float)rawPeriod) / 9.5f;
  if (cadence < 40.0f) {
    resetPowerAverage();
    powerWatts = 0;
    return;
  }

  int adjustment = offsetValid ? (int)gearOffset : 0;
  if (adjustment < -30 || adjustment > 30) adjustment = 0;

  const float coord =
      (100.0f * (float)(delta + adjustment)) / (float)span;
  const float sensorRpm = cadence * 9.5f;

  float slope;
  float intercept;

  if (coord > 105.0f) {
    intercept = -148.969909668f;
    slope = 1.086400032f;
  } else if (coord <= 0.0f) {
    intercept = 1.77060008049f;
    slope = 0.03709999844f;
  } else {
    int index = (int)(coord * 2.0f + 0.5f);
    if (index < 0) index = 0;
    if (index > 210) index = 210;

    slope = pgm_read_float(&kIc6PowerCurveTable[index][0]) + 0.006000000052f;
    intercept = pgm_read_float(&kIc6PowerCurveTable[index][1]) + 2.700000048f;
  }

  static const float chargerCorrection[4] = {0.0f, 0.5f, 2.5f, 5.0f};
  const uint8_t c = chargerState > 3 ? 3 : chargerState;

  float instant = intercept + sensorRpm * slope + chargerCorrection[c];
  if (instant < 0.0f) instant = 0.0f;

  float smoothed = averagePower(instant);
  if (smoothed < 0.0f) smoothed = 0.0f;
  if (smoothed > 32767.0f) smoothed = 32767.0f;

  powerWatts = (int16_t)(smoothed + 0.5f);
}

static void readStaticCalibration()
{
  uint8_t cal[11];
  if (readPower(0x31, cal, sizeof(cal))) {
    calStart = ((uint16_t)cal[3] << 8) | cal[4];
    calEnd   = ((uint16_t)cal[5] << 8) | cal[6];
    calibrationValid = (cal[9] == 1 && calStart != calEnd);
  } else {
    calibrationValid = false;
  }

  uint8_t off[10];
  if (readPower(0x33, off, sizeof(off)) && off[3] == 1) {
    int8_t candidate = (int8_t)off[8];
    if (candidate >= -30 && candidate <= 30) {
      gearOffset = candidate;
      offsetValid = true;
    } else {
      offsetValid = false;
    }
  } else {
    offsetValid = false;
  }
}

static const int CARD_W = 75;
static const int CARD_H = 88;
static const int CARD_BORDER = 3;  // Medium-weight colored frame.
static const int CARD_Y_TOP = 32;
static const int CARD_Y_BOTTOM = 128;

static void fillRoundedRect(int x, int y, int w, int h, int radius, int color)
{
  if (radius < 1) {
    lcd.color(color);
    lcd.xy(x, y);
    lcd.box(w, h, FILL);
    return;
  }

  lcd.color(color);

  // Two overlapping fills form the body; four small filled circles soften
  // only the extreme corners, keeping the tile mostly square/compact.
  lcd.xy(x + radius, y);
  lcd.box(w - 2 * radius, h, FILL);

  lcd.xy(x, y + radius);
  lcd.box(w, h - 2 * radius, FILL);

  lcd.circle(x + radius,         y + radius,         radius, FILL);
  lcd.circle(x + w - radius - 1, y + radius,         radius, FILL);
  lcd.circle(x + radius,         y + h - radius - 1, radius, FILL);
  lcd.circle(x + w - radius - 1, y + h - radius - 1, radius, FILL);
}

static void drawCard(int x, int y, int w, int h,
                     int accent, const char *line1, const char *line2)
{
  const int radius = 5;
  fillRoundedRect(x, y, w, h, radius, accent);
  fillRoundedRect(x + CARD_BORDER,
                  y + CARD_BORDER,
                  w - 2 * CARD_BORDER,
                  h - 2 * CARD_BORDER,
                  radius - CARD_BORDER,
                  BLACK);

  putCenteredSmall(x, w, y + 8,  WHITE, line1, false);
  putCenteredSmall(x, w, y + 22, WHITE, line2, false);
}

static void clearCardValue(int x, int y)
{
  lcd.color(BLACK);
  lcd.xy(x + CARD_BORDER, y + 45);
  lcd.box(CARD_W - 2 * CARD_BORDER,
          CARD_H - 45 - CARD_BORDER,
          FILL);
}

static void putCardValue(int x, int y, int color, const char *txt)
{
  clearCardValue(x, y);
  lcd.font(1);
  putCenteredSmall(x, CARD_W, y + 58, color, txt, false);
}

static void formatMilliVolts(uint16_t millivolts, char *out, size_t outLen)
{
  // Round to two decimal places without enabling printf float support on AVR.
  const uint16_t hundredths = (millivolts + 5U) / 10U;
  snprintf(out, outLen, "%u.%02u V",
           hundredths / 100U,
           hundredths % 100U);
}

static void drawStaticScreen()
{
  lcd.cls(BLACK);

  lcd.font(2);
  putText(12, 4, WHITE, "Life Fitness IC6 | I2C Test 0x40 0x48");
  putText(13, 4, WHITE, "Life Fitness IC6 | I2C Test 0x40 0x48");

  // 4 x 2 dashboard, matching the photographed IC6 test screen.
  // Colors intentionally alternate so every tile remains visually distinct.
  drawCard(  4, CARD_Y_TOP,    CARD_W, CARD_H, C_UI_BLUE,   "CADENCE",    "PEDAL SPEED");
  drawCard( 83, CARD_Y_TOP,    CARD_W, CARD_H, C_UI_YELLOW, "RESISTANCE", "LEVEL 0-100");
  drawCard(162, CARD_Y_TOP,    CARD_W, CARD_H, C_UI_RED,    "POWER",      "OUTPUT");
  drawCard(241, CARD_Y_TOP,    CARD_W, CARD_H, C_UI_GREEN,  "RAW",        "BRAKE");

  drawCard(  4, CARD_Y_BOTTOM, CARD_W, CARD_H, C_UI_GREEN,  "BATTERY",    "STATE");
  drawCard( 83, CARD_Y_BOTTOM, CARD_W, CARD_H, C_UI_BLUE,   "MOTOR",      "VOLTAGE");
  drawCard(162, CARD_Y_BOTTOM, CARD_W, CARD_H, C_UI_YELLOW, "BATTERY",    "VOLTAGE");
  drawCard(241, CARD_Y_BOTTOM, CARD_W, CARD_H, C_UI_RED,    "SYSTEM",     "VOLTAGE");
}

static void updateGearCards()
{
  char b[24];

  if (gearSeen) {
    snprintf(b, sizeof(b), "%u.%u rpm",
             cadence10 / 10U, cadence10 % 10U);
    putCardValue(4, CARD_Y_TOP, C_UI_BLUE, b);

    if (resistance >= 0) {
      snprintf(b, sizeof(b), "%d", resistance);
      putCardValue(83, CARD_Y_TOP, C_UI_YELLOW, b);
    } else {
      putCardValue(83, CARD_Y_TOP, SILVER, "--");
    }

    if (powerWatts >= 0) {
      snprintf(b, sizeof(b), "%d W", powerWatts);
      putCardValue(162, CARD_Y_TOP, C_UI_RED, b);
    } else {
      putCardValue(162, CARD_Y_TOP, SILVER, "--");
    }

    snprintf(b, sizeof(b), "%u", rawBrake);
    putCardValue(241, CARD_Y_TOP, C_UI_GREEN, b);
  } else {
    putCardValue(4,   CARD_Y_TOP, SILVER, "--");
    putCardValue(83,  CARD_Y_TOP, SILVER, "--");
    putCardValue(162, CARD_Y_TOP, SILVER, "--");
    putCardValue(241, CARD_Y_TOP, SILVER, "--");
  }
}

static void updatePowerCards()
{
  char b[24];

  if (powerStateValid) {
    snprintf(b, sizeof(b), "%u", batteryState);
    putCardValue(4, CARD_Y_BOTTOM, C_UI_GREEN, b);
  } else {
    putCardValue(4, CARD_Y_BOTTOM, SILVER, "--");
  }

  if (supplyValid) {
    // IC6 Power Module command 0x04:
    // S1 = UGEN/generator supply (shown as MOTOR VOLTAGE to match the old screen)
    // S2 = battery voltage
    // S3 = internal/system rail
    formatMilliVolts(supply1, b, sizeof(b));
    putCardValue(83, CARD_Y_BOTTOM, C_UI_BLUE, b);

    formatMilliVolts(supply2, b, sizeof(b));
    putCardValue(162, CARD_Y_BOTTOM, C_UI_YELLOW, b);

    formatMilliVolts(supply3, b, sizeof(b));
    putCardValue(241, CARD_Y_BOTTOM, C_UI_RED, b);
  } else {
    putCardValue(83,  CARD_Y_BOTTOM, SILVER, "--");
    putCardValue(162, CARD_Y_BOTTOM, SILVER, "--");
    putCardValue(241, CARD_Y_BOTTOM, SILVER, "--");
  }
}

static void updateScreen()
{
  updateGearCards();
  updatePowerCards();
}

void setup()
{
  lcd.begin(115200L);
  delay(150);
  lcd.light(100);
  lcd.colorID(C_UI_BLUE,     0,   0, 255);
  lcd.colorID(C_UI_YELLOW, 210, 180,   0);
  lcd.colorID(C_UI_RED,    225,   0,   0);
  lcd.colorID(C_UI_GREEN,    0, 170,   0);

  drawStaticScreen();

  Wire.begin();

  // 50 kHz at 16 MHz, prescaler 1.
  TWSR &= ~(_BV(TWPS0) | _BV(TWPS1));
  TWBR = 152;

  updateScreen();
}

void loop()
{
  const uint32_t now = millis();

  if (!staticLoaded || now - lastStaticPoll >= 30000UL) {
    lastStaticPoll = now;
    staticLoaded = true;
    readStaticCalibration();
    updateProcessedRideValues();
  }

  if (now - lastGearPoll >= 250UL) {
    lastGearPoll = now;

    uint8_t brakeReply[6];
    uint8_t cadenceReply[6];

    const bool brakeOK = readGear(0x02, brakeReply, sizeof(brakeReply));
    const bool cadenceOK = readGear(0x03, cadenceReply, sizeof(cadenceReply));

    gearSeen = brakeOK && cadenceOK;

    if (brakeOK)
      rawBrake = ((uint16_t)brakeReply[3] << 8) | brakeReply[4];

    if (cadenceOK) {
      rawPeriod = ((uint16_t)cadenceReply[3] << 8) | cadenceReply[4];
      if (rawPeriod != 0)
        cadence10 = (uint16_t)(4800000UL / (19UL * rawPeriod));
      else
        cadence10 = 0;
    }

    updateProcessedRideValues();
    updateGearCards();
  }

  if (now - lastPowerPoll >= 1000UL) {
    lastPowerPoll = now;

    uint8_t stateReply[6];
    uint8_t supplyReply[10];

    powerStateValid = readPower(0x06, stateReply, sizeof(stateReply));
    if (powerStateValid) {
      chargerState = stateReply[3];
      batteryState = stateReply[4];
      if (chargerState > 3 || batteryState > 2)
        powerStateValid = false;
    }

    supplyValid = readPower(0x04, supplyReply, sizeof(supplyReply));
    if (supplyValid) {
      supply1 = ((uint16_t)supplyReply[3] << 8) | supplyReply[4];
      supply2 = ((uint16_t)supplyReply[5] << 8) | supplyReply[6];
      supply3 = ((uint16_t)supplyReply[7] << 8) | supplyReply[8];
    }

    updateProcessedRideValues();
    updateGearCards();
    updatePowerCards();
  }
}
