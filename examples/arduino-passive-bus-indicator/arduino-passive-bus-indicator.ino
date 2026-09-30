/*
  Life Fitness / ICG IC6 passive I2C bus activity indicator

  Purpose:
    - basic wiring/activity check only
    - does not transmit on the I2C bus
    - does not decode packets or identify addresses

  Intended for a 5 V-compatible Arduino connected to the IC6 bus.

  Wiring:
    IC6 GND -> Arduino GND
    IC6 SDA -> Arduino SDA
    IC6 SCL -> Arduino SCL

  SDA and SCL are configured as plain INPUTs, with no internal pull-ups
  enabled. The sketch watches for logic transitions and flashes LED_BUILTIN
  when activity is observed.

  For a 3.3 V-only controller, use a proper bidirectional I2C level shifter.
*/

#include <Arduino.h>

static const uint16_t LED_HOLD_MS = 50;

int lastSDA;
int lastSCL;
uint32_t lastActivityMs;

void setup()
{
  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);

  lastSDA = digitalRead(SDA);
  lastSCL = digitalRead(SCL);
  lastActivityMs = 0;
}

void loop()
{
  const int sda = digitalRead(SDA);
  const int scl = digitalRead(SCL);

  if (sda != lastSDA || scl != lastSCL)
  {
    lastSDA = sda;
    lastSCL = scl;
    lastActivityMs = millis();
  }

  const bool active =
      (millis() - lastActivityMs) < LED_HOLD_MS;

  digitalWrite(
      LED_BUILTIN,
      active ? HIGH : LOW);
}
