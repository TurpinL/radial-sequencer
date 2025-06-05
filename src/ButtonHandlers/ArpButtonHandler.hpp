#pragma once

#include "IButtonHandler.hpp"

class ArpButtonHandler : public IButtonHandler {
public:
    void handle(
        UserInputState &userInputState, 
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) override;

    bool shouldSuppressCursorRotation() const override { return true; };
private:
};