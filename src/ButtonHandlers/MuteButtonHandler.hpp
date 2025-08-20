#pragma once

#include "IButtonHandler.hpp"

class MuteButtonHandler : public IButtonHandler {
public:
    void handle(
        UserInputState &userInputState,
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) override;

    bool shouldSuppressCursorRotation() const override { return false; };
};