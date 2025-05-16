#include "PitchButtonHandler.hpp"

void PitchButtonHandler::handle(
    Button &button, 
    SineCosinePot &endlessPot, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    for (auto stage : selectionState.getAffectedStages()) {
        float newVoltage = stage->output + endlessPot.getAngleDelta() / 360.f;
        stage->output = coerceInRange(newVoltage, 0, 2);
    }

    _pitchChange += endlessPot.getAngleDelta();
      
    if (button.fallingEdge()) {
        if (_pitchChange != 0) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
        _pitchChange = 0;
    }
}