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
    MOVE,
    DELETE,
    UNDO,
    REDO,
    ARP,
    NOTHING
};

inline String toString(Command command) {
    switch(command) {
        case PITCH:    return String("Pitch ");
        case SKIP:     return String("Skip  ");
        case SELECT:   return String("Select");
        case SLIDE:    return String("Slide ");
        case PULSES:   return String("Pulses");
        case GATEMODE: return String("Mode  ");
        case CLONE:    return String("Clone ");
        case MOVE:     return String("Move  ");
        case DELETE:   return String("Delete");
        case UNDO:     return String("Undo  ");
        case REDO:     return String("Redo  ");
        case ARP:      return String("Arp   ");
        case NOTHING:  return String("None  ");
        default:       return String("N/A   ");
    }
}

class Button {
    public:
        Button(Command command) {
            _command = command;
        }

        void update(bool isPressed) {
            _wasDoubleTapped = false;
            _didDoubleTapWindowPass = false;
            _lastState = _state;
            _state = (_state << 1) + isPressed;

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

        // To be called at the start of each tick so that
        // risingEdge or fallingEdge don't stay high
        // for multiple ticks
        void stabilizeState() {
            _lastState = _state;
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

        Command getCommand() {
            return _command;
        }

        Command _command;
    private:
        uint8_t _state = 0;
        uint8_t _lastState = 0;
        unsigned long _lastActivation = 0;
        bool _wasDoubleTapped = false;
        bool _isPrimedForDoubleTap = false;
        bool _didDoubleTapWindowPass = false;
};