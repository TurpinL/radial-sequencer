#pragma once

#include "Sequence.h"
#include "Button.h"
#include "UserInputState.hpp"
#include "SelectionState.hpp"
#include "ButtonHandlers/PitchButtonHandler.hpp"
#include "ButtonHandlers/SelectButtonHandler.hpp"
#include "ButtonHandlers/GateModeButtonHandler.hpp"
#include "ButtonHandlers/ArpButtonHandler.hpp"

class InteractionManager {
public:
    InteractionManager();
    void processInput(UndoRedoManager &undoRedoManager, UserInputState &userInputState);

    bool _isEditingPosition = false;
    uint8_t _highlightedStageIndex = 0;
    float _cursorAngle = 0;
    // These shouldn't be needed once all the buttons have dedicated handlers
    float _hiddenValue = 0;
    float _mutationCanary = 0;

    GateModeButtonHandler gateModeButtonHandler;
    SelectButtonHandler selectButtonHandler;
    PitchButtonHandler pitchButtonHandler;
    ArpButtonHandler arpButtonHandler;
    std::map<Command, IButtonHandler*> buttonHandlers;
};