#pragma once

#include "../Utils.h"
#include "../Button.h"
#include "../Sequence.h"
#include "../SineCosinePot.h"
#include "../UndoRedoManager.h"
#include "../SelectionState.hpp"

class PitchButtonHandler {
    public:
        void handle(
            Button &button, 
            SineCosinePot &endlessPot, 
            UndoRedoManager &undoRedoManager, 
            SelectionState &selectionState
        );
  
        bool shouldSuppressCursorRotation() { return true; };

    private:
        float _pitchChange = 0; // Used to detect if the state has been mutated
};