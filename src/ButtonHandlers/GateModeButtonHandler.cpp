#include "GateModeButtonHandler.hpp"

void GateModeButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        _isEditingGateMode = true;
    }

    _foo += userInputState.getAngleDelta();

    for (auto stage : selectionState.getAffectedStages()) {
        stage->pulsePipsAngle = wrapDeg(stage->pulsePipsAngle + userInputState.getAngleDelta());
        stage->gateMode = (GateMode)((int)round(wrapDeg(stage->pulsePipsAngle) / 90.f) % 4);
    }

    bool hasModified = (int)round(wrapDeg(_foo) / 90.f) % 4;
      
    if (userInputState.getBaseButton().fallingEdge()) {
        if (hasModified != 0) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
        _isEditingGateMode = false;
    }
}