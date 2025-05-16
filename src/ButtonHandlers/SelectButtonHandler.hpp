#pragma once

#include "../Utils.h"
#include "../Button.h"
#include "../Sequence.h"
#include "../SineCosinePot.h"
#include "../UndoRedoManager.h"
#include "../SelectionState.hpp"

class SelectButtonHandler {
    public:
        void handle(
            Button &button, 
            Button *modifier, 
            SineCosinePot &endlessPot, 
            UndoRedoManager &undoRedoManager, 
            SelectionState &selectionState
        );
    
        bool shouldSuppressCursorRotation() { return false; };

    private:
        bool _lastSelectToggleState = false;
  };