#pragma once

#include <Arduino.h>

#define DOUBLE_TAP_WINDOW_MS 200

class Button {
    public:
        Button(uint gpio) {
            _gpio = gpio;
        }

        void update() {
            _wasDoubleTapped = false;
            _didDoubleTapWindowPass = false;
            _lastState = _state;
            _state = (_state << 1) + gpio_get(_gpio);

            if (risingEdge()) {
                if (millis() - _lastActivation <= DOUBLE_TAP_WINDOW_MS) {
                    _wasDoubleTapped = true;
                    _isPrimedForDoubleTap = false;
                }
                
                _lastActivation = millis();
                _isPrimedForDoubleTap = !_wasDoubleTapped; 
            }

            if (millis() - _lastActivation > DOUBLE_TAP_WINDOW_MS && _isPrimedForDoubleTap) {
                _didDoubleTapWindowPass = true;
                _isPrimedForDoubleTap = false;
            }
        }

        bool held() {
            return _state == 0b11111111;
        }

        bool risingEdge() {
            return _lastState == 0b01111111 && _state == 0b11111111;
        }

        bool fallingEdge() {
            return _lastState == 0b11111111 && _state == 0b11111110;
        }

        bool doubleTapped() {
            return _wasDoubleTapped;
        }

        bool didDoubleTapWindowPass() {
            return _didDoubleTapWindowPass;
        }

    private:
        uint _gpio;
        uint8_t _state;
        uint8_t _lastState;
        unsigned long _lastActivation;
        bool _wasDoubleTapped;
        bool _isPrimedForDoubleTap = false;
        bool _didDoubleTapWindowPass;
};