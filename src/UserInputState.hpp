#pragma once

#include "Button.h"
#include "SineCosinePot.h"

#define BUTTON_COUNT 16

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
    QUANTIZER,
    MUTE,
    LENGTH,
    RANDOMIZE,
    NOTHING
};

inline String toString(Command command) {
    switch(command) {
        case PITCH:    return String("Pitch");
        case SKIP:     return String("Skip");
        case SELECT:   return String("Select");
        case SLIDE:    return String("Slide");
        case PULSES:   return String("Pulses");
        case GATEMODE: return String("Mode");
        case CLONE:    return String("Clone");
        case MOVE:     return String("Move");
        case DELETE:   return String("Delete");
        case UNDO:     return String("Undo");
        case REDO:     return String("Redo");
        case ARP:      return String("Arp");
        case QUANTIZER:return String("Quant");
        case MUTE:     return String("Mute");
        case LENGTH:   return String("Length");
        case RANDOMIZE:return String("Random");
        case NOTHING:  return String("None");
        default:       return String("N/A");
    }
}

class UserInputState {
    public:
        UserInputState(
            SineCosinePot *endlessPot, 
            int8_t activeButtonIndexes[2], 
            Button buttons[BUTTON_COUNT], 
            Command buttonMapping[BUTTON_COUNT]
        );

        UserInputState() {};

        Button &getBaseButton() { return *_baseButton; }
        Command getBaseCommand() { return _baseCommand; }
        Button &getModifierButton() { return *_modifierButton; }
        Command getModifierCommand() { return _modifierCommand; }
        float getAngleDelta() { return _endlessPotAngleDelta; }
    private:
        float _endlessPotAngleDelta = 0;
        Button *_baseButton = nullptr;
        Command _baseCommand = NOTHING;
        Button *_modifierButton = nullptr;
        Command _modifierCommand = NOTHING;
};