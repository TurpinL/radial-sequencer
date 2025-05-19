#pragma once

#include "IButtonHandler.hpp"

class GateModeButtonHandler : public IButtonHandler {
public:
    void handle(
        UserInputState &userInputState,
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) override;

    bool shouldSuppressCursorRotation() const override { return true; };
    bool isEditingGateMode() { return _isEditingGateMode; };

private:
    bool _isEditingGateMode = false;
    float _foo = 0;
};