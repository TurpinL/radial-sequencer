#include "ArpButtonHandler.hpp"

void ArpButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        for (auto stage : selectionState.getAffectedStages()) {
            stage->shouldArpeggiate = !stage->shouldArpeggiate;
        }

        undoRedoManager.saveUndoRedoSnapshot();
    }
}