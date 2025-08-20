#include "RandomizeButtonHandler.hpp"

void RandomizeButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    if (userInputState.getBaseButton().risingEdge()) {
        for (auto stage : selectionState.getAffectedStages()) {
            stage->setOutput((rand() % 100) / 50.f);
        }

        undoRedoManager.saveUndoRedoSnapshot();
    }
}