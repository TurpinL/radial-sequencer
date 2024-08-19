#pragma once

#include <Arduino.h>
#include "utils.h"

const uint adcChannelToPin[3] = {26, 27, 28};

class SineCosinePot {
  public:
    SineCosinePot(uint adcChannelA, uint adcChannelB) {
      _adcChannelA = adcChannelA;
      _adcPinA = adcChannelToPin[adcChannelA];

      _adcChannelB = adcChannelB;
      _adcPinB =  adcChannelToPin[adcChannelB];
    }

    ~SineCosinePot() {}

    void init() {
      adc_gpio_init(_adcPinA);
      adc_gpio_init(_adcPinB);
    }

    void update() {
      adc_select_input(_adcChannelA);
      uint16_t adcA = adc_read();

      adc_select_input(_adcChannelB);
      uint16_t adcB = adc_read();

      // Lowpass filter the results with a lerp
      float halfAngleFromAdcA = lerp(adcA / 22.7555555556, _lastHalfAngleA, 0.99f);
      float halfAngleFromAdcB = lerp(adcB / 22.7555555556, _lastHalfAngleB, 0.99f);
      _lastHalfAngleA = halfAngleFromAdcA;
      _lastHalfAngleB = halfAngleFromAdcB;

      float angleFromAdcA = 360 - ((halfAngleFromAdcB <= 90) ? halfAngleFromAdcA : 360 - halfAngleFromAdcA);
      angleFromAdcA = fwrap(angleFromAdcA + 90, 0, 360);
      float angleFromAdcB = (halfAngleFromAdcA <= 90) ? halfAngleFromAdcB : 360 - halfAngleFromAdcB;

      float lastAngle = _angle;

      // Avoid the deadzones around 0 and 180 in the potentiometer tracks 
      float a = 1 - abs(halfAngleFromAdcA - 90) / 90.f;
      float b = 1 - abs(halfAngleFromAdcB - 90) / 90.f;
      _angle = lerp(angleFromAdcB, angleFromAdcA, a / (float)(a + b));
      _angleDelta = degBetweenAngles(lastAngle, _angle);
    }

    float getAngle() { return _angle; }
    float getAngleDelta() { return _angleDelta; }

  private:
    uint _adcChannelA;
    uint _adcPinA;
    uint _adcChannelB;
    uint _adcPinB;
    float _angle;
    float _angleDelta;
    float _lastHalfAngleA;
    float _lastHalfAngleB;
};