#pragma once

#include <Arduino.h>

class Button {
    public:
        Button(uint gpio) {
            _gpio = gpio;
        }

        void update() {
            _lastState = _state;
            _state = (_state << 1) + gpio_get(_gpio);
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

    private:
        uint _gpio;
        uint8_t _state;
        uint8_t _lastState;
};