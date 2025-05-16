#pragma once

#include "Sequence.h"

class SelectionState {
    public:
        SelectionState(Sequence &sequence, uint8_t highlightedStageIndex);

        std::vector<Stage*> &getAffectedStages() { return _affectedStages; };
        std::vector<Stage*> &getSelectedStages() { return _selectedStages; };
        Stage *getHighlightedStage() { return _highlightedStage; };
    private: 
        Stage *_highlightedStage;
        std::vector<Stage*> _selectedStages;
        std::vector<Stage*> _affectedStages;
};