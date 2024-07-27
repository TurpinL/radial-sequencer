#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "sineCosinePot.h"

float a = 0;
float cursorAngle = 0;

SineCosinePot endlessPot = SineCosinePot(0, 1);

void setup() {
  Serial.begin(115200);

  adc_init();
  endlessPot.init();
}

void loop() {
  endlessPot.update();

  a += endlessPot.getAngleDelta();

  Serial.println(a);
}