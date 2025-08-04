#include "InteractionManager.hpp"

InteractionManager::InteractionManager() {
    buttonHandlers = {
        {GATEMODE, &gateModeButtonHandler},
        {SELECT, &selectButtonHandler},
        {PITCH, &pitchButtonHandler},
        {ARP, &arpButtonHandler}
    };
}

void InteractionManager::processQuantizerConfigInput(UndoRedoManager &undoRedoManager, UserInputState &userInputState) {
  if (userInputState.getBaseCommand() == QUANTIZER) {
    // Exit quantizer config
    if (userInputState.getBaseButton().fallingEdge()) {
      undoRedoManager.isInQuantizerConfig = false;
    }
  } else if (userInputState.getBaseCommand() == SELECT) {
    // Toggle current pip
    if (userInputState.getBaseButton().fallingEdge()) {
      int selectedNote = (int)round(_quantizerConfigCursorPos) % 12;
      undoRedoManager.getSequence()->quantizer[selectedNote] = !undoRedoManager.getSequence()->quantizer[selectedNote];
    }
  }

  _quantizerConfigCursorPos += userInputState.getAngleDelta() / 360.f * 12.f;
  _quantizerConfigCursorPos = fwrap(_quantizerConfigCursorPos, 0, 12); 
}

void InteractionManager::processInput(UndoRedoManager &undoRedoManager, UserInputState &userInputState) {
  Sequence &sequence = *undoRedoManager.getSequence();

  // Hidden value used for... things?
  bool shouldResetHiddenValue = true;

  // Update highlighted stage
  float degreesPerStage = 360 / (float)sequence.stageCount();

  if (!_isEditingPosition) {
    _highlightedStageIndex = (int)roundf(_cursorAngle / degreesPerStage) % sequence.stageCount();
  }

  SelectionState selectionState = SelectionState(sequence, _highlightedStageIndex); 

  bool isShiftHeld = false;
  bool shouldSupressCursorRotation = false;
  _isEditingPosition = false;

  if (undoRedoManager.isInQuantizerConfig) {
    shouldSupressCursorRotation = true;
    processQuantizerConfigInput(undoRedoManager, userInputState);
  } else if (buttonHandlers.find(userInputState.getBaseCommand()) != buttonHandlers.end()) {
    IButtonHandler &handler = *buttonHandlers[userInputState.getBaseCommand()];

    handler.handle(userInputState, undoRedoManager, selectionState);
    shouldSupressCursorRotation = handler.shouldSuppressCursorRotation();
  } else if (userInputState.getBaseCommand() == UNDO) {
    if (userInputState.getBaseButton().risingEdge()) {
      undoRedoManager.undo();
    } 
  } else if (userInputState.getBaseCommand() == REDO) {
    if (userInputState.getBaseButton().risingEdge()) {
      undoRedoManager.redo();
    } 
  } else if (userInputState.getBaseCommand() == SKIP) {
    if (userInputState.getBaseButton().risingEdge()) {
      // Toggle isSkipped
      for (auto stage : selectionState.getAffectedStages()) {
        stage->isSkipped = !stage->isSkipped;
      }
  
      sequence.updateNextStageIndex();

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (userInputState.getBaseCommand() == PULSES) {
    shouldSupressCursorRotation = true;
    shouldResetHiddenValue = false;

    _hiddenValue += userInputState.getAngleDelta();

    if (abs(_hiddenValue) > 20) {
      for (auto stage : selectionState.getAffectedStages()) {
        stage->pulseCount = coerceInRange(stage->pulseCount + (_hiddenValue > 0 ? 1 : -1), 1, 8);
      }
      _mutationCanary += (_hiddenValue > 0 ? 1 : -1);

      _hiddenValue = 0;
    }

    if (userInputState.getBaseButton().fallingEdge() && _mutationCanary != 0) {
      undoRedoManager.saveUndoRedoSnapshot();
      _mutationCanary = 0;
    }
  } else if (userInputState.getBaseCommand() == SLIDE) {
    if (userInputState.getBaseButton().risingEdge()) {
      for (auto stage : selectionState.getAffectedStages()) {
        stage->shouldSlideIn = !stage->shouldSlideIn;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (userInputState.getBaseCommand() == CLONE) {
    if (userInputState.getBaseButton().risingEdge()) {
      // Iterate from the end to the beginning because deleting 
      // stages moves around the data in the stages vector causing 
      // the pointers in affected stages to point to the wrong stage. 
      // Iterating in reverse is a workaround to that issue
      auto rit = selectionState.getAffectedStages().rbegin();
      while (rit != selectionState.getAffectedStages().rend()) {
        // Copy everything about the stage, except deselect it
        Stage newStage = **rit;
        newStage.isSelected = false;
        newStage.id = sequence.getNewStageId();

        sequence.insertStage(sequence.indexOfStage(*rit) + 1, newStage);
        
        ++rit;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (userInputState.getBaseCommand() == DELETE) {
    if (userInputState.getBaseButton().risingEdge()) {
      // Iterate from the end to the beginning because deleting 
      // stages moves around the data in the stages vector causing 
      // the pointers in affected stages to point to the wrong stage. 
      // Iterating in reverse is a workaround to that issue
      auto rit = selectionState.getAffectedStages().rbegin();
      while (rit != selectionState.getAffectedStages().rend()) {
        sequence.deleteStage(sequence.indexOfStage(*rit));
        ++rit;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (userInputState.getBaseCommand() == MOVE) {
    shouldResetHiddenValue = false;
    _isEditingPosition = true;

    _hiddenValue += userInputState.getAngleDelta();
    float degreesPerStage = 360 / (float)sequence.stageCount();

    // When we move the stages far enough rearrange the sequence
    if (abs(_hiddenValue) > degreesPerStage) {
      int direction = (_hiddenValue > 0) ? 1 : -1;
      _mutationCanary += direction;
      
      std::vector<size_t> indexesOfStagesToMove;
      for (Stage* stage : selectionState.getAffectedStages()) {
        size_t stageIndex = sequence.indexOfStage(stage);
        indexesOfStagesToMove.push_back(stageIndex);
      }

      sequence.moveStages(indexesOfStagesToMove, direction);

      // The highlightedStage will be moved by this action, so update the index
      _highlightedStageIndex = wrap((int)_highlightedStageIndex + direction, 0, sequence.stageCount());

      _hiddenValue = 0;
    }

    if (userInputState.getBaseButton().fallingEdge() && _mutationCanary != 0) {
      undoRedoManager.saveUndoRedoSnapshot();
      _mutationCanary = 0;
    }
  } else if (userInputState.getBaseCommand() == QUANTIZER) {
    if (userInputState.getBaseButton().fallingEdge()) {
      undoRedoManager.isInQuantizerConfig = true;
    }
  }

  if (!shouldSupressCursorRotation) {
    // Cusor
    _cursorAngle += userInputState.getAngleDelta();
    _cursorAngle = fwrap(_cursorAngle, 0, 360); 
  }

  if (shouldResetHiddenValue) {
    _hiddenValue = 0;
  }
}