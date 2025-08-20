#pragma once

#include "IButtonHandler.hpp"

class RandomizeButtonHandler : public IButtonHandler {
public:
    void handle(
        UserInputState &userInputState,
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) override;

    bool shouldSuppressCursorRotation() const override { return false; };
};