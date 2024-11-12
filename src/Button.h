#pragma once

#include <Arduino.h>
#include <string>

#define DOUBLE_TAP_WINDOW_MS 200

enum Command {
    PITCH,
    PULSES,
    GATEMODE,
    SKIP,
    SELECT,
    SLIDE,
    CLONE,
    DELETE,
    NOTHING
};

String toString(Command command) {
    switch(command) {
        case PITCH:    return String("Pitch ");
        case SKIP:     return String("Skip  ");
        case SELECT:   return String("Select");
        case SLIDE:    return String("Slide ");
        case PULSES:   return String("Pulses");
        case GATEMODE: return String("Mode  ");
        case CLONE:    return String("Clone  ");
        case DELETE:   return String("Delete  ");
        case NOTHING:  return String("None  ");
    }
}

class Button {
    public:
        Button(uint gpio, Command command) {
            _gpio = gpio;
            _command = command;
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

        unsigned long getLastActivation() {
            return _lastActivation;
        }
        Command _command;
    private:
        uint _gpio;
        uint8_t _state;
        uint8_t _lastState;
        unsigned long _lastActivation;
        bool _wasDoubleTapped;
        bool _isPrimedForDoubleTap = false;
        bool _didDoubleTapWindowPass;
};