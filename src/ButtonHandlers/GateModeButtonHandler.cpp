#include "GateModeButtonHandler.hpp"

void GateModeButtonHandler::handle(
    Button &button, 
    SineCosinePot &endlessPot, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (button.risingEdge()) {
        _isEditingGateMode = true;
    }

    _foo += endlessPot.getAngleDelta();

    for (auto stage : selectionState.getAffectedStages()) {
        stage->pulsePipsAngle = wrapDeg(stage->pulsePipsAngle + endlessPot.getAngleDelta());
        stage->gateMode = (GateMode)((int)round(wrapDeg(stage->pulsePipsAngle) / 90.f) % 4);
    }

    bool hasModified = (int)round(wrapDeg(_foo) / 90.f) % 4;
      
    if (button.fallingEdge()) {
        if (hasModified != 0) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
        _isEditingGateMode = false;
    }
}