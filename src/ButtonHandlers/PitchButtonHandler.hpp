#pragma once

#include "IButtonHandler.hpp"

class PitchButtonHandler : public IButtonHandler {
public:
    void handle(
        UserInputState &userInputState, 
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) override;

    bool shouldSuppressCursorRotation() const override { return true; };
    bool isEditingPitch() { return _isEditingPitch; }
private:
    float _pitchChange = 0; // Used to detect if the state has been mutated
    bool _isEditingPitch = false;
};