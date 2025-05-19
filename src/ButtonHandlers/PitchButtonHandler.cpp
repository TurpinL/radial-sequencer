#include "PitchButtonHandler.hpp"

void PitchButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        _isEditingPitch = true;
    }

    for (auto stage : selectionState.getAffectedStages()) {
        float newVoltage = stage->output + userInputState.getAngleDelta() / 360.f;
        stage->output = coerceInRange(newVoltage, 0, 2);
    }

    _pitchChange += userInputState.getAngleDelta();
      
    if (userInputState.getBaseButton().fallingEdge()) {
        if (_pitchChange != 0) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
        _pitchChange = 0;
        _isEditingPitch = false;
    }
}