#include "SelectButtonHandler.hpp"

void SelectButtonHandler::handle(
    UserInputState &userInputState, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    Sequence *sequence = undoRedoManager.getSequence();

    if (userInputState.getModifierCommand() == NOTHING) {
        if (userInputState.getBaseButton().risingEdge()) {
            if (userInputState.getBaseButton().doubleTapped()) { 
                // Select all or clear selection on double tap
                bool isHighlightedStageTheOnlySelectedStage = (selectionState.getSelectedStages().size() == 1 && selectionState.getHighlightedStage()->isSelected);
                bool shouldSelectStages = selectionState.getSelectedStages().size() == 0 || isHighlightedStageTheOnlySelectedStage;
    
                for (size_t i = 0; i < sequence->stageCount(); i++) {
                    sequence->getStage(i).isSelected = shouldSelectStages;
                }
            } else {
                // Toggle selection on highlit stage
                selectionState.getHighlightedStage()->isSelected = !selectionState.getHighlightedStage()->isSelected;
                _lastSelectToggleState = selectionState.getHighlightedStage()->isSelected;
            }
        }
  
        // Select multiple stages as you turn the knob
        selectionState.getHighlightedStage()->isSelected = _lastSelectToggleState;
  
        if (userInputState.getBaseButton().fallingEdge()) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
    } else if (userInputState.getModifierCommand() == SLIDE) {
        // Select every stage with slide toggled on
        if (userInputState.getModifierButton().risingEdge()) {
            for (size_t i = 0; i < sequence->stageCount(); i++) {
            sequence->getStage(i).isSelected = sequence->getStage(i).shouldSlideIn;
            }
            
            undoRedoManager.saveUndoRedoSnapshot();
        }
    }
}