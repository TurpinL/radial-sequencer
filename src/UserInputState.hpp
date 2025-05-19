#pragma once

#include "Button.h"
#include "SineCosinePot.h"

class UserInputState {
    public:
        UserInputState(SineCosinePot *endlessPot, std::vector<Button*> &activeButtons);

        Button &getBaseButton() { return *_baseButton; }
        Command getBaseCommand() { return _baseCommand; }
        Button &getModifierButton() { return *_modifierButton; }
        Command getModifierCommand() { return _modifierCommand; }
        SineCosinePot &getEndlessPot() { return *_endlessPot; }
        float getAngleDelta() { return _endlessPot->getAngleDelta(); }
    private:
        SineCosinePot *_endlessPot;
        std::vector<Button*> _activeButtons;
        Button *_baseButton = nullptr;
        Command _baseCommand = NOTHING;
        Button *_modifierButton = nullptr;
        Command _modifierCommand = NOTHING;
};