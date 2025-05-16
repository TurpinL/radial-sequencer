#include "SelectButtonHandler.hpp"

void SelectButtonHandler::handle(
    Button &button,
    Button *modifier, 
    SineCosinePot &endlessPot, 
    UndoRedoManager &undoRedoManager, 
    SelectionState &selectionState
) {
    Sequence *sequence = undoRedoManager.getSequence();

    if (modifier == nullptr) {
        if (button.risingEdge()) {
            if (button.doubleTapped()) { 
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
  
        if (button.fallingEdge()) {
            undoRedoManager.saveUndoRedoSnapshot();
        }
    } else if (modifier->getCommand() == SLIDE) {
        // Select every stage with slide toggled on
        if (modifier->risingEdge()) {
            for (size_t i = 0; i < sequence->stageCount(); i++) {
            sequence->getStage(i).isSelected = sequence->getStage(i).shouldSlideIn;
            }
            
            undoRedoManager.saveUndoRedoSnapshot();
        }
    }
}