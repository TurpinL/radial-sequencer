#include "UserInputState.hpp"

UserInputState::UserInputState(
    SineCosinePot *endlessPot, 
    int8_t activeButtonIndexes[2], 
    Button buttons[BUTTON_COUNT], 
    Command buttonMapping[BUTTON_COUNT]
) {
    _endlessPotAngleDelta = endlessPot->getAngleDelta();

    if (activeButtonIndexes[0] != -1) {
        _baseButton = &buttons[activeButtonIndexes[0]];
        _baseCommand = buttonMapping[activeButtonIndexes[0]];
    } else {
        _baseButton = nullptr;
        _baseCommand = NOTHING;
    }

    if (activeButtonIndexes[1] != -1) {
        _modifierButton = &buttons[activeButtonIndexes[1]];
        _modifierCommand = buttonMapping[activeButtonIndexes[1]];
    } else {
        _modifierButton = nullptr;
        _modifierCommand = NOTHING;
    }
}