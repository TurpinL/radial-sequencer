#pragma once

#include "../Utils.h"
#include "../Button.h"
#include "../Sequence.h"
#include "../SineCosinePot.h"
#include "../UndoRedoManager.h"
#include "../SelectionState.hpp"

class GateModeButtonHandler {
    public:
        void handle(
            Button &button, 
            SineCosinePot &endlessPot, 
            UndoRedoManager &undoRedoManager, 
            SelectionState &selectionState
        );
  
        bool shouldSuppressCursorRotation() { return true; };
        bool isEditingGateMode() { return _isEditingGateMode; };
        
    private:
        bool _isEditingGateMode = false;
        float _foo = 0;
};