#pragma once

#include <Arduino.h>
#include <string>

#define DOUBLE_TAP_WINDOW_MS 200

class Button {
    public:
        void update(bool isPressed) {
            _wasDoubleTapped = false;
            _didDoubleTapWindowPass = false;
            _lastState = _state;
            _state = (_state << 1) + isPressed;

            if (!_isHeld && _lastState == 0b01111111 && _state == 0b11111111) { // Detect rising edge
                _isHeld = true;
                _isRisingEdge = true;
                if (millis() - _lastActivation <= DOUBLE_TAP_WINDOW_MS) {
                    _wasDoubleTapped = true;
                    _isPrimedForDoubleTap = false;
                }
                
                _lastActivation = millis();
                _isPrimedForDoubleTap = !_wasDoubleTapped; 
            } else if (_isHeld && _lastState == 0b10000000 && _state == 0b00000000) { // Detect falling edge
                _isHeld = false;
                _isFallingEdge = true;
            }

            if (millis() - _lastActivation > DOUBLE_TAP_WINDOW_MS && _isPrimedForDoubleTap) {
                _didDoubleTapWindowPass = true;
                _isPrimedForDoubleTap = false;
            }
        }

        // To be called at the start of each tick so that
        // risingEdge or fallingEdge don't stay high
        // for multiple ticks.
        // Must be called before update
        void stabilizeState() {
            _lastState = _state;
            _isRisingEdge = false;
            _isFallingEdge = false;
        }

        bool held() {
            return _isHeld;
        }

        bool risingEdge() {
            return _isRisingEdge;
        }

        bool fallingEdge() {
            return _isFallingEdge;
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
    private:
        uint8_t _state = 0;
        uint8_t _lastState = 0;
        unsigned long _lastActivation = 0;
        bool _wasDoubleTapped = false;
        bool _isPrimedForDoubleTap = false;
        bool _didDoubleTapWindowPass = false;
        bool _isHeld = false;
        bool _isRisingEdge = false;
        bool _isFallingEdge = false;
};