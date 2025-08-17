#pragma once

#include "../utils.h"
#include "../Sequence.h"
#include "../UserInputState.hpp"
#include "../UndoRedoManager.hpp"
#include "../SelectionState.hpp"

class IButtonHandler {
public:
    virtual ~IButtonHandler() = default;

    virtual void handle(
        UserInputState &userInputState, 
        UndoRedoManager &undoRedoManager, 
        SelectionState &selectionState
    ) = 0;

    virtual bool shouldSuppressCursorRotation() const = 0;
};