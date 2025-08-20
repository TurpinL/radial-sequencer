#include "MuteButtonHandler.hpp"

void MuteButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        for (auto stage : selectionState.getAffectedStages()) {
            if (stage->gateMode == NONE) {
                stage->gateMode = HELD;
            } else {
                stage->gateMode = NONE;
            }
        }

        undoRedoManager.saveUndoRedoSnapshot();
    }
}