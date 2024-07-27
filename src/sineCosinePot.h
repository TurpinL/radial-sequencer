#ifndef RAD_SEQ_SINECOSINE_POT
#define RAD_SEQ_SINECOSINE_POT

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

      float halfAngleFromAdcA = adcA / 22.7555555556;
      float halfAngleFromAdcB = adcB / 22.7555555556;

      float angleFromAdcA = 360 - ((halfAngleFromAdcB <= 90) ? halfAngleFromAdcA : 360 - halfAngleFromAdcA);
      angleFromAdcA = fwrap(angleFromAdcA + 90, 0, 360);
      float angleFromAdcB = (halfAngleFromAdcA <= 90) ? halfAngleFromAdcB : 360 - halfAngleFromAdcB;

      float lastAngle = _angle;
      _angle = lerp(angleFromAdcA, angleFromAdcB, adcA / (float)(adcA + adcB));
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
};

#endif