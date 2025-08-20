#include "LengthButtonHandler.hpp"

void LengthButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        for (auto stage : selectionState.getAffectedStages()) {
            if (stage->pulseCount == 1) {
                stage->pulseCount = 2;
            } else {
                stage->pulseCount = 1;
            }
        }

        undoRedoManager.saveUndoRedoSnapshot();
    }
}