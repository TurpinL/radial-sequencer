#include "SelectionState.hpp"

SelectionState::SelectionState(Sequence &sequence, uint8_t highlightedStageIndex) {
    _highlightedStage = &sequence.getStage(highlightedStageIndex);
    _selectedStages = sequence.getSelectedStages();

    _affectedStages = _selectedStages;
    if (!_highlightedStage->isSelected) {
        // Also affect the highlighted stage
        _affectedStages.push_back(_highlightedStage);
    }
}